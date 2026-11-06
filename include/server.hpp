#pragma once

#include "event_loop.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include <thread>
#include "awaitable.hpp"

namespace FishRPC { 

  class RpcServer{
  public:

    
  private:
  
    void start_once(const char* port)
    {
      int listen_fd = create_listen_socket(port);
      event_loop loop;


      loop.run(task_listence(loop, listen_fd));
      close(listen_fd);
    }


    task<void> task_listence(event_loop& loop, int fd){
      io_uring ring;
      
      sockaddr_in addr;
      socklen_t addr_len;

      while(1){
        int fd = co_await task_accept(&ring, fd, &addr, &addr_len);
        loop.add_task();

      }

    }

    task<void> handle_connection(int fd)
    {
      
      


    }

    



    std::vector<std::thread> threads;

  };

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