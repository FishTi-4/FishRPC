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

  task<bool> connect(const char* ip, const char* port){

    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC , 0);
    if(fd < 0)  throw std::runtime_error("socket error");

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(port));
    if(inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) throw std::runtime_error("inet_pton error");

    int res = co_await task_connect(&loop.get_ring(), (sockaddr*)&addr, sizeof(addr), fd);

    if(res < 0) throw std::runtime_error("connect error");
    
    server_fd = fd;
    loop.add_task(reader(fd));
    co_return true;
  }

  task<package_name::Resp> call(std::string method, package_name::Frame& REQ){ 
    if(server_fd <= 0)  throw std::runtime_error("not connected");

    package_name::Resp resp;
    int cur_id = idx ++;
    resp.set_id(cur_id);

    auto req = REQ.mutable_request();
    req->set_method(method);
    req->set_id(cur_id);
    
    awaitable_response awaiter;
    pending_[resp.id()] = &awaiter;


    if(!co_await write_frame(loop.get_ring(), server_fd, REQ)){
      throw std::runtime_error("write request error");
    }

    co_await awaiter;

    pending_.erase(cur_id);

    if(!resp.ParseFromString(awaiter.result)){
      throw std::runtime_error("parse response error");
    }
    else co_return resp;
  }

  void run(task<void>&& main_task){
    loop.run(std::move(main_task));

  }


private:
  int server_fd = -1;
  event_loop loop;
  int idx = 0;

  task<void> reader(int fd){ 
    while(true){ 
      package_name::Frame content;
      if(!co_await read_frame(loop.get_ring(), fd, content)) break;
      
      auto resp = content.mutable_response();
      auto it = pending_.find(resp->id());

      if(it == pending_.end()) throw std::runtime_error("unknown request id");
      
      auto& awaiter = *it->second;
      awaiter.error = resp->error();
      awaiter.error_message = resp->error_message();
      awaiter.result = resp->result();

      awaiter.ready = 1;
      awaiter.handle_.resume();
    }

    co_return;
  }

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






}