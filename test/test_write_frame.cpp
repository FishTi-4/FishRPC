#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "protocol.hpp"

namespace {

using FishRPC::awaitable_base;
using FishRPC::task;

using Frame = package_name::Frame;

// 手动驱动协程直至完成（与 event_loop::handle_task 同样的逻辑）：
// resume -> submit_and_wait -> 逐条处理 cqe（回填 result 并恢复协程）
// 返回值是处理过的 cqe 个数，可用来观察实际发生了多少次写操作。
template <typename T>
int pump(io_uring* ring, task<T> t, T* out) {
  int cqe_count = 0;

  t.resume();
  while (!t.is_done()) {
    int ret = io_uring_submit_and_wait(ring, 1);
    assert(ret >= 0);

    io_uring_cqe* cqe;
    unsigned head = 0; // 迭代游标（liburing>=2.14 语义为绝对下标，不再是 cqe 个数）
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

Frame make_req_frame(int id, const std::string& method, const std::string& params) {
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

// 阻塞读取固定字节数（write_frame 已把数据写入内核缓冲，这里不会阻塞）
std::string read_all(int fd, size_t size) {
  std::string buf(size, '\0');
  size_t got = 0;
  while (got < size) {
    ssize_t n = ::read(fd, buf.data() + got, size - got);
    assert(n > 0);
    got += static_cast<size_t>(n);
  }
  return buf;
}

// 校验线上格式: [4 字节网络序长度][protobuf 载荷]
void check_wire_format(const std::string& wire, const Frame& expected) {
  std::string payload;
  expected.SerializeToString(&payload);

  assert(wire.size() == 4 + payload.size());

  uint32_t len_net = 0;
  std::memcpy(&len_net, wire.data(), 4);
  assert(ntohl(len_net) == payload.size());

  assert(std::memcmp(wire.data() + 4, payload.data(), payload.size()) == 0);
}

} // namespace

// 1. 基本用例：写入一个 Req 帧，校验线上格式且返回 true
void test_write_frame_basic() {
  int sv[2];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  io_uring ring;
  assert(io_uring_queue_init(64, &ring, 0) == 0);

  Frame frame = make_req_frame(42, "hello", "world");
  std::string payload;
  frame.SerializeToString(&payload);

  bool ok = false;
  int cqe_count = pump(&ring, FishRPC::write_frame(ring, sv[0], frame), &ok);
  assert(ok);
  assert(cqe_count == 1); // 载荷很小，一次写即可完成

  std::string wire = read_all(sv[1], 4 + payload.size());
  check_wire_format(wire, frame);

  // 反序列化校验内容
  Frame parsed;
  assert(parsed.ParseFromString(wire.substr(4)));
  assert(parsed.has_request());
  assert(parsed.request().id() == 42);
  assert(parsed.request().method() == "hello");
  assert(parsed.request().params() == "world");

  close(sv[0]);
  close(sv[1]);
  io_uring_queue_exit(&ring);
}

// 2. 空帧：只写出 4 字节的 0 长度头
void test_write_frame_empty() {
  int sv[2];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  io_uring ring;
  assert(io_uring_queue_init(64, &ring, 0) == 0);

  Frame frame; // 未设置任何字段

  bool ok = false;
  pump(&ring, FishRPC::write_frame(ring, sv[0], frame), &ok);
  assert(ok);

  std::string wire = read_all(sv[1], 4);
  assert(wire == std::string(4, '\0'));
  check_wire_format(wire, frame);

  close(sv[0]);
  close(sv[1]);
  io_uring_queue_exit(&ring);
}

// 3. Response 帧（oneof 的另一分支）
void test_write_frame_response() {
  int sv[2];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  io_uring ring;
  assert(io_uring_queue_init(64, &ring, 0) == 0);

  Frame frame = make_resp_frame(9, 0, "ok");
  std::string payload;
  frame.SerializeToString(&payload);

  bool ok = false;
  pump(&ring, FishRPC::write_frame(ring, sv[0], frame), &ok);
  assert(ok);

  std::string wire = read_all(sv[1], 4 + payload.size());
  check_wire_format(wire, frame);

  Frame parsed;
  assert(parsed.ParseFromString(wire.substr(4)));
  assert(parsed.has_response());
  assert(parsed.response().id() == 9);
  assert(parsed.response().error() == 0);
  assert(parsed.response().result() == "ok");

  close(sv[0]);
  close(sv[1]);
  io_uring_queue_exit(&ring);
}

// 4. 部分写：缩小发送/接收缓冲并让对端缓慢排水，
//    一次写不完时应循环写直至全部发出，且数据完整
void test_write_frame_partial() {
  int sv[2];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  int small = 4096;
  setsockopt(sv[0], SOL_SOCKET, SO_SNDBUF, &small, sizeof(small));
  setsockopt(sv[1], SOL_SOCKET, SO_RCVBUF, &small, sizeof(small));

  io_uring ring;
  assert(io_uring_queue_init(64, &ring, 0) == 0);

  // 载荷远大于缓冲容量，保证出现部分写
  Frame frame = make_req_frame(7, "big", std::string(512 * 1024, 'x'));
  std::string payload;
  frame.SerializeToString(&payload);
  const size_t total = 4 + payload.size();

  // 读取线程缓慢排水，迫使写入端多次部分写
  std::string received(total, '\0');
  std::thread reader([&] {
    size_t got = 0;
    while (got < total) {
      ssize_t n = ::read(sv[1], received.data() + got, total - got);
      assert(n > 0);
      got += static_cast<size_t>(n);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });

  bool ok = false;
  int cqe_count = pump(&ring, FishRPC::write_frame(ring, sv[0], frame), &ok);
  assert(ok);
  assert(cqe_count >= 2); // 确实发生过多次写

  reader.join();
  check_wire_format(received, frame);

  close(sv[0]);
  close(sv[1]);
  io_uring_queue_exit(&ring);
}

// 5. 写失败：无效 fd，应返回 false 而不是崩溃/死循环
void test_write_frame_bad_fd() {
  io_uring ring;
  assert(io_uring_queue_init(64, &ring, 0) == 0);

  Frame frame = make_req_frame(1, "m", "p");

  bool ok = true;
  int cqe_count = pump(&ring, FishRPC::write_frame(ring, -1, frame), &ok);
  assert(!ok);
  assert(cqe_count == 1); // 第一次写就失败，不会继续循环

  io_uring_queue_exit(&ring);
}

// 6. 同一连接连续写多个帧，帧边界应可正确解析
void test_write_multiple_frames() {
  int sv[2];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  io_uring ring;
  assert(io_uring_queue_init(64, &ring, 0) == 0);

  Frame a = make_req_frame(1, "add", "1+2");
  Frame b = make_req_frame(2, "mul", "3*4");

  bool ok1 = false;
  bool ok2 = false;
  pump(&ring, FishRPC::write_frame(ring, sv[0], a), &ok1);
  pump(&ring, FishRPC::write_frame(ring, sv[0], b), &ok2);
  assert(ok1);
  assert(ok2);

  // 对端按帧手动解析
  auto read_one = [](int fd) {
    std::string hdr = read_all(fd, 4);
    uint32_t len_net = 0;
    std::memcpy(&len_net, hdr.data(), 4);
    return read_all(fd, ntohl(len_net));
  };

  Frame pa, pb;
  assert(pa.ParseFromString(read_one(sv[1])));
  assert(pb.ParseFromString(read_one(sv[1])));
  assert(pa.request().id() == 1 && pa.request().method() == "add");
  assert(pb.request().id() == 2 && pb.request().method() == "mul");

  close(sv[0]);
  close(sv[1]);
  io_uring_queue_exit(&ring);
}

int main() {
  test_write_frame_basic();
  test_write_frame_empty();
  test_write_frame_response();
  test_write_frame_partial();
  test_write_frame_bad_fd();
  test_write_multiple_frames();

  std::cout << "write_frame tests passed" << std::endl;
  return 0;
}
