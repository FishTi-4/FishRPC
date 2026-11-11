#pragma once

#include <coroutine>
#include <liburing.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>


namespace FishRPC{

  
  struct awaitable_base{

    
  enum class Type{
    destroy,
    resume
  };
  
  std::coroutine_handle<> handle;
  int result;
  Type type = Type::resume;
};


struct task_accept : public awaitable_base {
  
  io_uring* ring;
  int listen_fd;
  sockaddr_in* addr;
  socklen_t* addr_len;
  
  task_accept(io_uring* ring, int listen_fd, sockaddr_in* addr, socklen_t* addr_len);

  bool await_ready();

  void await_suspend(std::coroutine_handle<> handle);

  int await_resume();
  
};

struct task_read : public awaitable_base { 
  
  io_uring* ring;
  int fd;
  char* buf;
  int len;

  task_read(io_uring* ring, int fd, char* buf, int len);

  bool await_ready();

  void await_suspend(std::coroutine_handle<> handle);

  int await_resume();
};


struct task_write : public awaitable_base { 

  io_uring* ring;
  char* buf;
  int size;
  int fd;

  task_write(io_uring* ring, char* buf, int size, int fd);

  bool await_ready();

  void await_suspend(std::coroutine_handle<> handle);

  int await_resume();
};


  struct task_connect : public awaitable_base { 

    io_uring* ring;
    sockaddr* addr;
    socklen_t addr_len;
    int fd;

    task_connect(io_uring* ring, sockaddr* addr, socklen_t addr_len, int fd);

    bool await_ready();

    void await_suspend(std::coroutine_handle<> handle);

    int await_resume();

  };


} //namespace FishRPC
