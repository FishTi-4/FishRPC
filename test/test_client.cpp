// 极简 client 冒烟测试：
//   fork 一个子进程运行 RpcServer，父进程用 RpcClient 连接，
//   发一帧 echo 请求并校验回包。
//
// 注意：与 test_server.cpp 相同，server 的 start_once 是 private，
// 这里用局部宏打开访问权限。先包含所有依赖，避免 private 宏污染
// 标准库头文件。

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

#include "client.hpp"
#include "FishRPC.pb.h"

#define private public
#include "server.hpp"
#undef private

namespace {

constexpr int kTimeoutSec = 2;

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

// 客户端主流程：connect -> call -> 把响应写回 out。
FishRPC::task<void> client_flow(FishRPC::RpcClient& client, const std::string& port,
                                package_name::Resp& out) {
  bool ok = co_await client.connect("127.0.0.1", port.c_str());
  assert(ok);

  // call<REQ, RESP> 会把请求体序列化进 params（与 test_server.cpp 用法一致）：
  // server 侧 register_method 会 req.ParseFromString(params) 还原出 REQ。
  package_name::Req inner_req;
  inner_req.set_id(0);
  inner_req.set_method("echo");
  inner_req.set_params("hello");

  out = co_await client.call<package_name::Req, package_name::Resp>("echo", inner_req);
  co_return;
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

  // 先探测 server 是否开始监听。RpcClient::connect 失败会 throw，
  // 而当前 task 的 unhandled_exception 会 terminate，所以必须先等它就绪。
  int probe = connect_to(port, pid);
  assert(probe >= 0 && "server did not start listening");
  ::close(probe);

  FishRPC::RpcClient client;
  package_name::Resp resp;
  client.run(client_flow(client, port_str, resp));

  assert(resp.id() == 0);
  assert(resp.error() == 0);
  assert(resp.result() == "hello");

  ::kill(pid, SIGKILL);

  int status = 0;
  ::waitpid(pid, &status, 0);

  std::cout << "client smoke test passed" << std::endl;
  return 0;
}
