#include <algorithm>
#include <cassert>
#include <cstring>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include <liburing.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "protocol.hpp"

namespace {

using FishRPC::awaitable_base;
using FishRPC::read_exatc;

constexpr int kQueueSize = 1024;

// 模拟 event_loop::handle_task 的收尾逻辑：
// 提交并等待一个完成事件，把 cqe->res 写回 awaitable，
// 供协程在 await_resume 中读取。
void pump_io(io_uring& ring) {
  int ret = io_uring_submit_and_wait(&ring, 1);
  assert(ret >= 0);

  io_uring_cqe* cqe = nullptr;
  int err = io_uring_peek_cqe(&ring, &cqe);
  assert(err == 0);
  assert(cqe != nullptr);

  auto* aw = reinterpret_cast<awaitable_base*>(io_uring_cqe_get_data(cqe));
  aw->result = cqe->res;
  io_uring_cq_advance(&ring, 1);
}

// 驱动 read_exatc 协程直到完成。
// 每次 recv 完成前会调用 feeder()：可继续写入数据、关闭写端等。
bool drive_read(io_uring& ring, int fd, int size, std::string& buffer,
                const std::function<void()>& feeder) {
  auto t = read_exatc(ring, size, buffer, fd);
  assert(!t.is_done());

  t.resume();  // 运行到第一个 co_await，SQE 已入队

  while (!t.is_done()) {
    feeder();
    pump_io(ring);
    t.resume();
  }

  return t.get();
}

int send_all(int fd, const char* data, size_t len) {
  size_t off = 0;
  while (off < len) {
    ssize_t n = ::write(fd, data + off, len - off);
    if (n < 0) {
      return -1;
    }
    off += static_cast<size_t>(n);
  }
  return 0;
}

std::string make_random_data(size_t n, unsigned seed = 42) {
  std::mt19937 gen(seed);
  std::uniform_int_distribution<int> dist(0, 255);
  std::string s;
  s.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    s.push_back(static_cast<char>(dist(gen)));
  }
  return s;
}

// socketpair 辅助：sv[0] 写端，sv[1] 读端（传给 read_exatc）
struct sock_pair {
  int sv[2] = {-1, -1};

  sock_pair() {
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
  }

  ~sock_pair() {
    close();
  }

  sock_pair(const sock_pair&) = delete;
  sock_pair& operator=(const sock_pair&) = delete;

  int writer() const { return sv[0]; }
  int reader() const { return sv[1]; }

  void close() {
    if (sv[0] >= 0) {
      ::close(sv[0]);
      sv[0] = -1;
    }
    if (sv[1] >= 0) {
      ::close(sv[1]);
      sv[1] = -1;
    }
  }
};

}  // namespace

// 1. 数据一次性到达，一次 recv 读满
void test_read_small_exact() {
  io_uring ring{};
  assert(io_uring_queue_init(kQueueSize, &ring, 0) == 0);

  sock_pair sp;
  std::string data = make_random_data(100);
  assert(send_all(sp.writer(), data.data(), data.size()) == 0);

  std::string buffer;
  bool ok = drive_read(ring, sp.reader(), static_cast<int>(data.size()), buffer,
                       [] {});
  assert(ok);
  assert(buffer == data);

  io_uring_queue_exit(&ring);
}

// 2. 恰好等于 PROTOCOL_BUF_SIZE 的边界
void test_read_exact_buffer_size() {
  io_uring ring{};
  assert(io_uring_queue_init(kQueueSize, &ring, 0) == 0);

  sock_pair sp;
  std::string data = make_random_data(PROTOCOL_BUF_SIZE);
  assert(send_all(sp.writer(), data.data(), data.size()) == 0);

  std::string buffer;
  bool ok = drive_read(ring, sp.reader(), static_cast<int>(data.size()), buffer,
                       [] {});
  assert(ok);
  assert(buffer == data);

  io_uring_queue_exit(&ring);
}

// 3. 数据预先全部就绪，size 超过 4096，需要多次 recv
void test_read_large_prewritten() {
  io_uring ring{};
  assert(io_uring_queue_init(kQueueSize, &ring, 0) == 0);

  sock_pair sp;
  const size_t total = 10 * 1024;
  std::string data = make_random_data(total);
  assert(send_all(sp.writer(), data.data(), data.size()) == 0);

  std::string buffer;
  bool ok = drive_read(ring, sp.reader(), static_cast<int>(total), buffer,
                       [] {});
  assert(ok);
  assert(buffer == data);

  io_uring_queue_exit(&ring);
}

// 4. 数据分段到达：分 3 批写入，覆盖多段循环读取
void test_read_multi_segment() {
  io_uring ring{};
  assert(io_uring_queue_init(kQueueSize, &ring, 0) == 0);

  sock_pair sp;
  const size_t total = 10 * 1024;
  std::string data = make_random_data(total);

  const std::vector<size_t> chunks{3000, 5000, 2240};  // 合计 == total
  size_t sent = 0;
  size_t chunk_idx = 0;

  auto feeder = [&]() {
    if (chunk_idx < chunks.size()) {
      size_t n = chunks[chunk_idx++];
      assert(send_all(sp.writer(), data.data() + sent, n) == 0);
      sent += n;
    }
  };

  std::string buffer;
  bool ok = drive_read(ring, sp.reader(), static_cast<int>(total), buffer,
                       feeder);
  assert(ok);
  assert(sent == total);
  assert(buffer == data);

  io_uring_queue_exit(&ring);
}

// 5. 读到一半对端关闭：返回 false，已读到的部分保留在 buffer 中
void test_read_eof_after_partial() {
  io_uring ring{};
  assert(io_uring_queue_init(kQueueSize, &ring, 0) == 0);

  sock_pair sp;
  std::string data = make_random_data(100);
  assert(send_all(sp.writer(), data.data(), data.size()) == 0);

  bool shutdown_called = false;
  auto feeder = [&]() {
    if (!shutdown_called) {
      ::shutdown(sp.writer(), SHUT_WR);
      shutdown_called = true;
    }
  };

  std::string buffer;
  bool ok = drive_read(ring, sp.reader(), 1000, buffer, feeder);
  assert(!ok);
  assert(buffer == data);

  io_uring_queue_exit(&ring);
}

// 6. 对端直接关闭（无数据）：返回 false，buffer 为空
void test_read_eof_immediate() {
  io_uring ring{};
  assert(io_uring_queue_init(kQueueSize, &ring, 0) == 0);

  sock_pair sp;
  auto feeder = [&]() { ::shutdown(sp.writer(), SHUT_WR); };

  std::string buffer;
  bool ok = drive_read(ring, sp.reader(), 100, buffer, feeder);
  assert(!ok);
  assert(buffer.empty());

  io_uring_queue_exit(&ring);
}

// 7. size == 0 —— 已知问题，暂不测试：
//    read_exatc 在首次 co_await 时会提交 len == 0 的 recv，
//    而内核中零长度 recv 永远不会产生完成事件（已用最小复现验证），
//    因此 size == 0 时 read_exatc 会永久挂起。
//    建议在 read_exatc 开头加 `if (size <= 0) co_return true;` 之类的保护。

// 8. 追加语义：buffer 已有内容时不应被清空
void test_read_appends_to_buffer() {
  io_uring ring{};
  assert(io_uring_queue_init(kQueueSize, &ring, 0) == 0);

  sock_pair sp;
  std::string data = "hello";
  assert(send_all(sp.writer(), data.data(), data.size()) == 0);

  std::string buffer = "prefix:";
  bool ok = drive_read(ring, sp.reader(), static_cast<int>(data.size()),
                       buffer, [] {});
  assert(ok);
  assert(buffer == "prefix:hello");

  io_uring_queue_exit(&ring);
}

// 9. recv 出错（fd 已关闭）：read_exatc 抛异常，
//    当前 promise_type::unhandled_exception 会 std::terminate（SIGABRT）。
//    用 fork 隔离，子进程应被 SIGABRT 终止。
void test_read_error_aborts() {
  pid_t pid = ::fork();
  assert(pid >= 0);

  if (pid == 0) {
    // ---- 子进程 ----
    ::freopen("/dev/null", "w", stdout);
    ::freopen("/dev/null", "w", stderr);

    io_uring ring{};
    if (io_uring_queue_init(kQueueSize, &ring, 0) != 0) {
      ::_exit(1);
    }

    int sv[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
      ::_exit(1);
    }
    ::close(sv[0]);
    ::close(sv[1]);  // 读端已关闭，recv 将返回 -EBADF

    std::string buffer;
    auto t = read_exatc(ring, 10, buffer, sv[1]);
    t.resume();

    // 子进程内手动 pump，失败时用正常退出码区分，避免误判为 SIGABRT
    int ret = io_uring_submit_and_wait(&ring, 1);
    if (ret < 0) {
      ::_exit(2);
    }
    io_uring_cqe* cqe = nullptr;
    if (io_uring_peek_cqe(&ring, &cqe) != 0 || cqe == nullptr) {
      ::_exit(2);
    }
    auto* aw = reinterpret_cast<awaitable_base*>(io_uring_cqe_get_data(cqe));
    aw->result = cqe->res;
    io_uring_cq_advance(&ring, 1);

    t.resume();  // res < 0 → throw → unhandled_exception → terminate

    ::_exit(3);  // 不应执行到这里
  }

  // ---- 父进程 ----
  int status = 0;
  ::waitpid(pid, &status, 0);
  assert(WIFSIGNALED(status));
  assert(WTERMSIG(status) == SIGABRT);
}

int main() {
  test_read_small_exact();
  test_read_exact_buffer_size();
  test_read_large_prewritten();
  test_read_multi_segment();
  test_read_eof_after_partial();
  test_read_eof_immediate();
  test_read_appends_to_buffer();
  test_read_error_aborts();

  std::cout << "protocol tests passed" << std::endl;
  return 0;
}
