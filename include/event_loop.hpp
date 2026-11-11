#pragma once

#include "coroutine.hpp"
#include <liburing.h>
#include <unordered_map>
#include <awaitable.hpp>

namespace FishRPC{


  class event_loop{
  public:

    event_loop(const event_loop&) = delete;
    event_loop(event_loop&&) = delete;

    event_loop& operator=(const event_loop&) = delete;
    event_loop& operator=(event_loop&&) = delete;

    
    event_loop();
    ~event_loop();

    void run(task<void>&& main_task);

    void add_task(task<void> task);

    io_uring& get_ring();

  private:
    io_uring ring_{};

    void handle_task();
    void reap_task();


    std::unordered_map<void*, task<void>> tasks_;
  
  };


}
