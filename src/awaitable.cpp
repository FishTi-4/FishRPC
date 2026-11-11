#include "awaitable.hpp"


namespace FishRPC{

  task_accept::task_accept(io_uring* ring, int listen_fd, sockaddr_in* addr, socklen_t* addr_len)
  : ring(ring), listen_fd(listen_fd), addr(addr), addr_len(addr_len)
  {
  }
  
  bool task_accept::await_ready(){
    return false;
  }
  
  void task_accept::await_suspend(std::coroutine_handle<> handle){
    this->handle = handle;
    io_uring_sqe* sqe = io_uring_get_sqe(ring);
    io_uring_prep_accept(sqe, listen_fd, reinterpret_cast<sockaddr*>(addr), addr_len, 0);
    io_uring_sqe_set_data(sqe, this);
  }
  
  int task_accept::await_resume(){
    return result;
  }
  

  task_read::task_read(io_uring* ring, int fd, char* buf, int len)
  : ring(ring), fd(fd), buf(buf), len(len)
  {
  }

  bool task_read::await_ready(){
    return false;
  }

  void task_read::await_suspend(std::coroutine_handle<> handle){
    this->handle = handle;
    io_uring_sqe* sqe = io_uring_get_sqe(ring);
    io_uring_prep_recv(sqe, fd, buf, len ,0);
    io_uring_sqe_set_data(sqe, this);
  }

  int task_read::await_resume(){
    return result;
  }


  task_write::task_write(io_uring* ring, char* buf, int size, int fd)
  : ring(ring), buf(buf), size(size), fd(fd)
  {
  }

  bool task_write::await_ready(){
    return false;
  }

  void task_write::await_suspend(std::coroutine_handle<> handle){
    this->handle = handle;
    io_uring_sqe* sqe = io_uring_get_sqe(ring);
    io_uring_prep_write(sqe, fd, buf, size, 0);
    io_uring_sqe_set_data(sqe, this);
  }

  int task_write::await_resume(){
    return result;
  }


  task_connect::task_connect(io_uring* ring, sockaddr* addr, socklen_t addr_len, int fd)
  : ring(ring), addr(addr), addr_len(addr_len), fd(fd)
  {
  }

  bool task_connect::await_ready(){
    return false;
  }

  void task_connect::await_suspend(std::coroutine_handle<> handle){
    this->handle = handle;
    io_uring_sqe* sqe = io_uring_get_sqe(ring);
    io_uring_prep_connect(sqe, fd, addr, addr_len);
    io_uring_sqe_set_data(sqe, this);
  }

  int task_connect::await_resume(){
    return result;
  }


} //namespace FishRPC
