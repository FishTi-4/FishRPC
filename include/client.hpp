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

  template<typename REQ, typename RESP>
  task<RESP> call(std::string method, REQ& request){
    if(server_fd <= 0)  throw std::runtime_error("not connected");

    int cur_id = idx++;

    package_name::Frame frame;
    auto* req = frame.mutable_request();
    req->set_id(cur_id);
    req->set_method(method);

    std::string params;
    if(!request.SerializeToString(&params)){
      throw std::runtime_error("serialize request error");
    }
    req->set_params(params);

    awaitable_response awaiter;
    pending_[cur_id] = &awaiter;

    if(!co_await write_frame(loop.get_ring(), server_fd, frame)){
      throw std::runtime_error("write request error");
    }

    co_await awaiter;

    pending_.erase(cur_id);

    if(awaiter.error != 0){
      throw std::runtime_error(awaiter.error_message);
    }

    RESP resp;
    if(!resp.ParseFromString(awaiter.result)){
      throw std::runtime_error("parse response error");
    }

    co_return resp;
  }

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
