#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "protocol.hpp"

namespace {

using FishRPC::awaitable_base;
using FishRPC::Frame;
using FishRPC::task;

// 手动驱动协程直至完成（与 event_loop::handle_task 同样的逻辑）：
// resume -> submit_and_wait -> 逐条处理 cqe（回填 result 并恢复协程）。
// read_frame 内部会 co_await read_exatc，因此这个循环也会驱动嵌套协程。
template <typename T>
int pump(io_uring* ring, task<T> t, T* out) {
  int cqe_count = 0;

  t.resume();
  while (!t.is_done()) {
    int ret = io_uring_submit_and_wait(ring, 1);
    assert(ret >= 0);

    io_uring_cqe* cqe;
    unsigned head = 0;  // 迭代游标（liburing>=2.14 语义为绝对下标）
    unsigned cnt = 0;
    io_uring_for_each_cqe(ring, head, cqe) {
      ++cnt;
      ++cqe_count;
      auto* base = reinterpret_cast<awaitable_base*>(cqe->user_data);
      base->result = cqe->res;
      base->handle.resume();
    }
    io_uring_cq_advance(ring, cnt);
  }

  *out = t.get();
  return cqe_count;
}

bool read_frame_once(io_uring& ring, int fd, Frame& out) {
  bool ok = false;
  pump(&ring, FishRPC::read_frame(ring, fd, out), &ok);
  return ok;
}

Frame make_req_frame(int id, const std::string& method,
                     const std::string& params) {
  Frame frame;
  auto* req = frame.mutable_request();
  req->set_id(id);
  req->set_method(method);
  req->set_params(params);
  return frame;
}

Frame make_resp_frame(int id, int error, const std::string& result) {
  Frame frame;
  auto* resp = frame.mutable_response();
  resp->set_id(id);
  resp->set_error(error);
  resp->set_result(result);
  return frame;
}

// 按线上格式序列化: [4 字节网络序长度][protobuf 载荷]
std::string serialize_frame(const Frame& frame) {
  std::string payload;
  assert(frame.SerializeToString(&payload));

  uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
  std::string wire(reinterpret_cast<const char*>(&len), sizeof(len));
  wire += payload;
  return wire;
}

void send_raw(int fd, const std::string& data) {
  size_t sent = 0;
  while (sent < data.size()) {
    ssize_t n = ::write(fd, data.data() + sent, data.size() - sent);
    assert(n > 0);
    sent += static_cast<size_t>(n);
  }
}

void send_frame_blocking(int fd, const Frame& frame) {
  send_raw(fd, serialize_frame(frame));
}

}  // namespace

// 1. 基本用例：读取一个 Request 帧，字段完整
void test_read_frame_basic() {
  int sv[2];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  io_uring ring;
  assert(io_uring_queue_init(64, &ring, 0) == 0);

  Frame frame = make_req_frame(42, "hello", "world");
  send_frame_blocking(sv[0], frame);

  Frame got;
  assert(read_frame_once(ring, sv[1], got));
  assert(got.has_request());
  assert(got.request().id() == 42);
  assert(got.request().method() == "hello");
  assert(got.request().params() == "world");

  close(sv[0]);
  close(sv[1]);
  io_uring_queue_exit(&ring);
}

// 2. Response 帧（oneof 的另一分支）
void test_read_frame_response() {
  int sv[2];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  io_uring ring;
  assert(io_uring_queue_init(64, &ring, 0) == 0);

  Frame frame = make_resp_frame(9, 3, "error");
  send_frame_blocking(sv[0], frame);

  Frame got;
  assert(read_frame_once(ring, sv[1], got));
  assert(got.has_response());
  assert(got.response().id() == 9);
  assert(got.response().error() == 3);
  assert(got.response().result() == "error");

  close(sv[0]);
  close(sv[1]);
  io_uring_queue_exit(&ring);
}

// 3. 空帧：长度头为 0，载荷为空，解析成功
void test_read_frame_empty() {
  int sv[2];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  io_uring ring;
  assert(io_uring_queue_init(64, &ring, 0) == 0);

  send_raw(sv[0], std::string(4, '\0'));  // 4 字节零长度头

  Frame got;
  got.mutable_request()->set_id(123);  // 解析空帧应清掉旧内容
  assert(read_frame_once(ring, sv[1], got));
  assert(!got.has_request());
  assert(!got.has_response());

  close(sv[0]);
  close(sv[1]);
  io_uring_queue_exit(&ring);
}

// 4. 同一连接连续读多个帧，帧边界不能串
void test_read_multiple_frames() {
  int sv[2];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  io_uring ring;
  assert(io_uring_queue_init(64, &ring, 0) == 0);

  Frame a = make_req_frame(1, "add", "1+2");
  Frame b = make_req_frame(2, "mul", "3*4");
  send_frame_blocking(sv[0], a);
  send_frame_blocking(sv[0], b);

  Frame pa;
  Frame pb;
  assert(read_frame_once(ring, sv[1], pa));
  assert(read_frame_once(ring, sv[1], pb));
  assert(pa.request().id() == 1 && pa.request().method() == "add");
  assert(pb.request().id() == 2 && pb.request().method() == "mul");

  close(sv[0]);
  close(sv[1]);
  io_uring_queue_exit(&ring);
}

// 5. 大载荷：远超 PROTOCOL_BUF_SIZE，需要多次 recv 才能读满
void test_read_frame_large_payload() {
  int sv[2];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  // 缩小缓冲区，确保读端发生多次 recv
  int small = 4096;
  setsockopt(sv[0], SOL_SOCKET, SO_SNDBUF, &small, sizeof(small));
  setsockopt(sv[1], SOL_SOCKET, SO_RCVBUF, &small, sizeof(small));

  io_uring ring;
  assert(io_uring_queue_init(64, &ring, 0) == 0);

  Frame frame = make_req_frame(7, "big", std::string(128 * 1024, 'x'));

  // 发送线程独立运行，避免写满 socket 缓冲时互相等待
  std::thread writer([&] { send_frame_blocking(sv[0], frame); });

  Frame got;
  bool ok = false;
  int cqe_count = pump(&ring, FishRPC::read_frame(ring, sv[1], got), &ok);
  writer.join();

  assert(ok);
  assert(cqe_count >= 3);  // 1 次读头 + 至少 2 次读载荷
  assert(got.has_request());
  assert(got.request().id() == 7);
  assert(got.request().method() == "big");
  assert(got.request().params() == frame.request().params());

  close(sv[0]);
  close(sv[1]);
  io_uring_queue_exit(&ring);
}

// 6. 帧被拆成多段发送：长度头与载荷分离，甚至载荷再分段
void test_read_frame_fragmented() {
  int sv[2];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  io_uring ring;
  assert(io_uring_queue_init(64, &ring, 0) == 0);

  Frame frame = make_req_frame(11, "frag", std::string(4096, 'y'));
  std::string wire = serialize_frame(frame);

  std::thread writer([&] {
    // 先只发长度头，再分段发载荷
    send_raw(sv[0], wire.substr(0, 4));
    for (size_t off = 4; off < wire.size(); off += 1000) {
      send_raw(sv[0], wire.substr(off, 1000));
    }
  });

  Frame got;
  assert(read_frame_once(ring, sv[1], got));
  writer.join();

  assert(got.has_request());
  assert(got.request().id() == 11);
  assert(got.request().params() == frame.request().params());

  close(sv[0]);
  close(sv[1]);
  io_uring_queue_exit(&ring);
}

// 7. 对端在读长度头前关闭：返回 false
void test_read_frame_eof_before_header() {
  int sv[2];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  io_uring ring;
  assert(io_uring_queue_init(64, &ring, 0) == 0);

  ::shutdown(sv[0], SHUT_WR);

  Frame got;
  assert(!read_frame_once(ring, sv[1], got));

  close(sv[0]);
  close(sv[1]);
  io_uring_queue_exit(&ring);
}

// 8. 长度头只到了 2 字节就关闭：返回 false，不能把残缺头当长度
void test_read_frame_eof_partial_header() {
  int sv[2];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  io_uring ring;
  assert(io_uring_queue_init(64, &ring, 0) == 0);

  send_raw(sv[0], std::string("ab", 2));
  ::shutdown(sv[0], SHUT_WR);

  Frame got;
  assert(!read_frame_once(ring, sv[1], got));

  close(sv[0]);
  close(sv[1]);
  io_uring_queue_exit(&ring);
}

// 9. 载荷读到一半对端关闭：返回 false
void test_read_frame_eof_partial_payload() {
  int sv[2];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  io_uring ring;
  assert(io_uring_queue_init(64, &ring, 0) == 0);

  uint32_t len = htonl(16);
  std::string wire(reinterpret_cast<const char*>(&len), sizeof(len));
  wire += "only-five";  // 声明 16 字节，只给 9 字节
  send_raw(sv[0], wire);
  ::shutdown(sv[0], SHUT_WR);

  Frame got;
  assert(!read_frame_once(ring, sv[1], got));

  close(sv[0]);
  close(sv[1]);
  io_uring_queue_exit(&ring);
}

// 10. 载荷不是合法 protobuf：ParseFromString 失败，返回 false
void test_read_frame_invalid_payload() {
  int sv[2];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  io_uring ring;
  assert(io_uring_queue_init(64, &ring, 0) == 0);

  std::string payload("\x08", 1);  // 缺少 varint，解析必然失败
  uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
  std::string wire(reinterpret_cast<const char*>(&len), sizeof(len));
  wire += payload;
  send_raw(sv[0], wire);

  Frame got;
  assert(!read_frame_once(ring, sv[1], got));

  close(sv[0]);
  close(sv[1]);
  io_uring_queue_exit(&ring);
}

// 11. write_frame / read_frame 往返：线上格式互相兼容
void test_read_frame_roundtrip_with_write_frame() {
  int sv[2];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  io_uring ring;
  assert(io_uring_queue_init(64, &ring, 0) == 0);

  Frame sent = make_resp_frame(5, 0, "ok");
  bool write_ok = false;
  pump(&ring, FishRPC::write_frame(&ring, sv[0], sent), &write_ok);
  assert(write_ok);

  Frame got;
  assert(read_frame_once(ring, sv[1], got));
  assert(got.has_response());
  assert(got.response().id() == 5);
  assert(got.response().error() == 0);
  assert(got.response().result() == "ok");

  close(sv[0]);
  close(sv[1]);
  io_uring_queue_exit(&ring);
}

int main() {
  test_read_frame_basic();
  test_read_frame_response();
  test_read_frame_empty();
  test_read_multiple_frames();
  test_read_frame_large_payload();
  test_read_frame_fragmented();
  test_read_frame_eof_before_header();
  test_read_frame_eof_partial_header();
  test_read_frame_eof_partial_payload();
  test_read_frame_invalid_payload();
  test_read_frame_roundtrip_with_write_frame();

  std::cout << "read_frame tests passed" << std::endl;
  return 0;
}
