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
  
  task_accept(io_uring* ring, int listen_fd, sockaddr_in* addr, socklen_t* addr_len)
  : ring(ring), listen_fd(listen_fd), addr(addr), addr_len(addr_len)
  {
  }
  
  
  bool await_ready(){
    return false;
  }
  
  void await_suspend(std::coroutine_handle<> handle){
    io_uring_sqe* sqe = io_uring_get_sqe(ring);
    io_uring_prep_accept(sqe, listen_fd, reinterpret_cast<sockaddr*>(addr), addr_len, 0);
    io_uring_sqe_set_data(sqe, this);
  }
  
  int await_resume(){
    return result;
  }
  
};

struct task_read : public awaitable_base { 
  
  io_uring* ring;
  int fd;
  char* buf;
  int len;

  task_read(io_uring* ring, int fd, char* buf, int len)
  : ring(ring), fd(fd), buf(buf), len(len)
  {
  }


  bool await_ready(){
    return false;
  }

  void await_suspend(std::coroutine_handle<> handle){
    io_uring_sqe* sqe = io_uring_get_sqe(ring);
    io_uring_prep_recv(sqe, fd, buf, len ,0);
    io_uring_sqe_set_data(sqe, this);
  }

  int await_resume(){
    return result;
  }

};



} //namespace FishRPC