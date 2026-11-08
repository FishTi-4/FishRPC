#pragma once

#include "event_loop.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include <thread>
#include "awaitable.hpp"
#include "protocol.hpp"
#include "FishRPC.pb.h"
#include "unordered_map"

namespace FishRPC { 

  inline int create_listen_socket(const char* port);

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

    
  private:
  
    void start_once(const char* port)
    {
      int listen_fd = create_listen_socket(port);
      event_loop loop;


      loop.run(task_listence(loop, listen_fd, loop.get_ring()));
      close(listen_fd);
    }


    task<void> task_listence(event_loop& loop, int fd, io_uring& ring){
      
      sockaddr_in addr;
      socklen_t addr_len;

      while(1){
        int fd = co_await task_accept(&ring, fd, &addr, &addr_len);
        loop.add_task( frame_worker(ring, fd) );
      }
    }

    task<void> frame_worker (io_uring& ring, int fd){
      // using namespace package_name;
      
      while(true){

        package_name::Frame Req;
        read_frame(ring, fd, Req);
        
        package_name::Frame Resp = Req_handle(Req);
        write_frame(ring, fd, Resp);

      }

      close(fd);
    }

    

    package_name::Frame Req_handle(package_name::Frame& in){
      
      package_name::Frame out;
      auto resp = out.mutable_response();
      
      const auto& req = in.request();
      
      auto it = method_.find(req.method());
      resp->set_id(req.id());

      if(it == method_.end()){
        resp->set_error_message("method not found");
        resp->set_error(true);
      }
      else{
        std::string res = it->second(req.params());
        resp->set_result(res);
        resp->set_error(false);
      }
      
      return out;
    }


    
    
    
    std::vector<std::thread> threads;
    std::unordered_map<std::string, std::function<std::string(std::string)>> method_;
    
  };  // ============= class =============

  inline int create_listen_socket(const char* port)
  {
      int listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
      if (listen_fd < 0) 
        throw std::runtime_error("socket() failed");

      int opt = 1;
      setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
      setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = INADDR_ANY;
      addr.sin_port = htons(static_cast<uint16_t>(atoi(port)));

      if (bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
          close(listen_fd);
          throw std::runtime_error("bind() failed");
      }
      if (listen(listen_fd, 4096) < 0) {
          close(listen_fd);
          throw std::runtime_error("listen() failed");
      }
      return listen_fd;
  }

} //namespace