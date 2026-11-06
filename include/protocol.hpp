#pragma once

#include <string>
#include <arpa/inet.h>
#include "awaitable.hpp"
#include "coroutine.hpp"
#include "FishRPC.pb.h"

namespace FishRPC{

  bool read_exatc(int size, std::string &buffer, int fd){
    
    while(1){



    }
    
  }

  bool read_frame(){

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