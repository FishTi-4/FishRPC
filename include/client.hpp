#pragma once

#include "event_loop.hpp"
#include "awaitable.hpp"
#include <arpa/inet.h>
#include "FishRPC.pb.h"
#include "protocol.hpp"
#include "Task.pb.h"
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace FishRPC {


class RpcClient{
public:

  task<bool> connect(const char* ip, const char* port);

  task<package_name::Resp> call(std::string method, package_name::Frame& REQ);

  void run(task<void>&& main_task);


private:
  int server_fd = -1;
  event_loop loop;
  int idx = 0;

  task<void> reader(int fd);

  struct awaitable_response {
    int ready = 0;

    int code = 0;
    int error;
    std::string error_message;
    std::string result;

    std::coroutine_handle<> handle_;

    bool await_ready() {
      return ready;
    }

    void await_suspend(std::coroutine_handle<> handle) {
      handle_ = handle;
    }

    void await_resume() {
    }
  };

  std::unordered_map<int, awaitable_response*> pending_;

};


} // namespace FishRPC
