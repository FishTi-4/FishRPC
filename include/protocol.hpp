#pragma once

#include <string>
#include <liburing.h>
#include <algorithm>
#include <arpa/inet.h>
#include "awaitable.hpp"
#include "coroutine.hpp"
#include "FishRPC.pb.h"

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

  task<bool> read_frame(io_uring& ring, int size, std::string &buffer, int fd){

    

  }


  using Frame = package_name::Frame;

  task<bool> write_frame(io_uring* ring, int fd, Frame content){

    std::string load;
    content.SerializeToString(&load);

    std::string head;
    int len = load.size();
    
    len = htonl(len);
    head.append(reinterpret_cast<char*>(&len), 4);

    load = head + load;

    int send_size = 0;
    while(1){

      int res = co_await task_write(ring, load.data() + send_size, load.size() - send_size, fd);
      if(res <= 0){
        co_return false;
      }
      // else if(res == 0){
      //   break;
      // }
      
      send_size += res;
      if(send_size == load.size()){
        break;
      }
    }

    co_return true;

  }
  

  
} // namespace FishRPC;