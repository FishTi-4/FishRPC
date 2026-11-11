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


  task<bool> read_exatc(io_uring& ring, int size, std::string &buffer, int fd);

  // using Frame = package_name::Frame;

  task<bool> read_frame(io_uring& ring, int fd, package_name::Frame& content);

  task<bool> write_frame(io_uring& ring, int fd, package_name::Frame& content);

  

  
} // namespace FishRPC;
