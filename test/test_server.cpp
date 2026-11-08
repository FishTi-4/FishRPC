// 极简 server 冒烟测试：
//   fork 一个子进程运行 RpcServer，父进程通过 TCP 连上去，
//   发一帧 echo 请求并校验回包。
//
// 注意：当前 server.hpp 的 start_once 是 private，而本次改动约定
// 不修改 server.hpp，因此这里用局部宏打开访问权限。先包含所有依赖，
// 避免 private 宏污染标准库头文件。

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "awaitable.hpp"
#include "event_loop.hpp"
#include "protocol.hpp"
#include "FishRPC.pb.h"

#define private public
#include "server.hpp"
#undef private

namespace {

constexpr int kTimeoutSec = 2;

std::string wire_frame(const package_name::Frame& frame) {
  std::string payload;
  assert(frame.SerializeToString(&payload));

  uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
  std::string wire(reinterpret_cast<const char*>(&len), sizeof(len));
  wire += payload;
  return wire;
}

bool write_all(int fd, const std::string& data) {
  size_t sent = 0;
  while (sent < data.size()) {
    ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
    if (n <= 0) {
      return false;
    }
    sent += static_cast<size_t>(n);
  }
  return true;
}

bool read_all(int fd, char* dst, size_t size) {
  size_t got = 0;
  while (got < size) {
    ssize_t n = ::recv(fd, dst + got, size - got, 0);
    if (n <= 0) {
      return false;
    }
    got += static_cast<size_t>(n);
  }
  return true;
}

bool read_frame_blocking(int fd, package_name::Frame& out) {
  uint32_t len_net = 0;
  if (!read_all(fd, reinterpret_cast<char*>(&len_net), sizeof(len_net))) {
    return false;
  }

  uint32_t len = ntohl(len_net);
  if (len > (1u << 20)) {
    return false;
  }

  std::string payload(len, '\0');
  if (!read_all(fd, payload.data(), len)) {
    return false;
  }
  return out.ParseFromString(payload);
}

// 反复尝试连接，直到 server 开始监听或子进程退出。
int connect_to(uint16_t port, pid_t child) {
  for (int i = 0; i < 200; ++i) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      return -1;
    }

    timeval tv{kTimeoutSec, 0};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
      return fd;
    }
    ::close(fd);

    int status = 0;
    if (::waitpid(child, &status, WNOHANG) == child) {
      return -1;  // 子进程已经退出，不用再等
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return -1;
}

package_name::Resp echo(const package_name::Req& req) {
  package_name::Resp resp;
  resp.set_id(req.id());
  resp.set_result(req.params());
  return resp;
}

}  // namespace

int main() {
  const uint16_t port = static_cast<uint16_t>(20000 + (::getpid() % 20000));
  const std::string port_str = std::to_string(port);

  pid_t pid = ::fork();
  assert(pid >= 0);

  if (pid == 0) {
    // 父进程如果异常退出，内核会顺手杀掉本进程，避免残留 server。
    ::prctl(PR_SET_PDEATHSIG, SIGKILL);
    ::freopen("/dev/null", "w", stdout);
    ::freopen("/dev/null", "w", stderr);

    FishRPC::RpcServer server;
    server.register_method<package_name::Req, package_name::Resp>("echo", echo);
    server.start_once(port_str.c_str());  // 阻塞运行
    ::_exit(0);
  }

  int fd = connect_to(port, pid);
  assert(fd >= 0 && "server did not start listening");

  package_name::Frame req;
  auto* request = req.mutable_request();
  request->set_id(1);
  request->set_method("echo");

  // params 按协议约定携带"序列化后的请求体"（与 proto/Task.proto 的 ADD_REQ 用法一致）：
  // server 侧 register_method 会 req.ParseFromString(params) 还原出 REQ。
  package_name::Req inner_req;
  inner_req.set_id(1);
  inner_req.set_method("echo");
  inner_req.set_params("hello");
  std::string payload;
  assert(inner_req.SerializeToString(&payload));
  request->set_params(payload);

  assert(write_all(fd, wire_frame(req)) && "send request failed");

  package_name::Frame resp;
  assert(read_frame_blocking(fd, resp) && "server did not respond");

  assert(resp.has_response());
  assert(resp.response().id() == 1);
  assert(resp.response().error() == 0);

  package_name::Resp inner;
  assert(inner.ParseFromString(resp.response().result()));
  assert(inner.result() == "hello");

  ::close(fd);
  ::kill(pid, SIGKILL);

  int status = 0;
  ::waitpid(pid, &status, 0);

  std::cout << "server smoke test passed" << std::endl;
  return 0;
}
