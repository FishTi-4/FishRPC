#include "protocol.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace FishRPC{

  task<bool> read_exatc(io_uring& ring, int size, std::string &buffer, int fd){
    
    int got = 0;
    char buf[PROTOCOL_BUF_SIZE];

    if (size <= 0) co_return size == 0;

    while(1){
      int res = co_await task_read(&ring, fd, buf, std::min(PROTOCOL_BUF_SIZE, size - got ) ) ;
      if(res < 0){
        throw std::runtime_error("read error");
      }
      else if(res == 0){
        co_return false;
      }
      

      buffer.append(buf, res);
      got += res;
      
      if(got >= size){
        break;
      }
    }

    co_return true;
  }

  task<bool> read_frame(io_uring& ring, int fd, package_name::Frame& content){

    std::string s;
    if(co_await read_exatc(ring, 4, s,  fd) == false){
      co_return false;
    }
    
    int len;
    memcpy(&len, s.data(), 4);
    len = ntohl(len);

    s = "";
    co_await read_exatc(ring, len, s, fd);

    co_return content.ParseFromString(s);
  }

  task<bool> write_frame(io_uring& ring, int fd, package_name::Frame& content){

    std::string load;
    content.SerializeToString(&load);

    std::string head;
    int len = load.size();
    
    len = htonl(len);
    head.append(reinterpret_cast<char*>(&len), 4);

    load = head + load;

    int send_size = 0;
    while(1){

      int res = co_await task_write(&ring, load.data() + send_size, load.size() - send_size, fd);
      if(res < 0){
        co_return false;
      }
      else if(res == 0){
        co_return false;
      }
      
      send_size += res;
      if(send_size == load.size()){
        break;
      }
    }

    co_return true;

  }
  

  
} // namespace FishRPC;
