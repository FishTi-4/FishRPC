#pragma once

#include <string>
#include <liburing.h>
#include "coroutine.hpp"
#include "awaitable.hpp"
#include <algorithm>

namespace FishRPC{

#define PROTOCOL_BUF_SIZE 4096

  task<bool> read_exatc(io_uring& ring, int size, std::string &buffer, int fd){
    
    int got = 0;
    char buf[PROTOCOL_BUF_SIZE];

    while(1){
      int res = co_await task_read(&ring, fd, buf, std::min(PROTOCOL_BUF_SIZE, size - got ) ) ;
      if(res <= 0){
        throw std::runtime_error("read error");
      }
      

      buffer.append(buf, res);
      got += res;
      
      if(got >= size){
        break;
      }
    }

    co_return true;
  }

  bool read_frame(io_uring& ring, int size, std::string &buffer, int fd){

  }


  bool write_frame(){

  }
  


} // namespace FishRPC;