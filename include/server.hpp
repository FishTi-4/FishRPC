#pragma once

#include "event_loop.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include <thread>
#include <functional>
#include <string>
#include <utility>
#include "awaitable.hpp"
#include "protocol.hpp"
#include "FishRPC.pb.h"
#include <unordered_map>

namespace FishRPC { 

  int create_listen_socket(const char* port);

  class RpcServer{
  public:

    template<typename REQ, typename RESP, typename FN>
    void register_method(std::string name, FN&& fn){
      method_[std::move(name)] = [fn](std::string params) -> std::string {
        
        REQ req;
        req.ParseFromString(params);

        RESP resp = fn(req);
        std::string out;
        resp.SerializeToString(&out);

        return out;
      };
    }

    void start(const char* port, int thread_num);

    
  private:
  
    void start_once(const char* port);


    task<void> task_listence(event_loop& loop, int fd, io_uring& ring);


    task<void> frame_worker (io_uring& ring, int fd);


    package_name::Frame Req_handle(package_name::Frame& in);


    
    
    
    std::vector<std::thread> threads;
    std::unordered_map<std::string, std::function<std::string(std::string)>> method_;
    
  };  // ============= class =============

} //namespace
