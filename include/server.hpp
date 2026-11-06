#pragma once

#include "event_loop.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include <thread>

namespace FishRPC { 

  class RpcServer{
  public:

    
  private:
  
    void start_once(const char* port)
    {
      int listen_fd = create_listen_socket(port);
      event_loop loop;


      loop.run(task_listence (listen_fd));
      close(listen_fd);
    }

    task<void> task_listence(int fd){

      
      while(1){

        co_await task_accept(fd);  

      }

    }

    class task_accept{
    public:
    
      task_accept()
      {
        io_uring_queue_init(1024, &ring, 0);
      }

      io_uring get_ring()
      {
        return ring;
      }

      bool await_ready()
      {
        return false;
      }

      void await_suspend(std::coroutine_handle<> handle)
      {
        io_uring_sqe* sqe = io_uring_get_sqe(&ring);
        io_uring_prep_accept(sqe, fd, nullptr, nullptr, 0);
        io_uring_sqe_set_data(sqe, handle.address());
        io_uring_submit(&ring);
      }

      


    
    private:
      io_uring ring;
    };



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