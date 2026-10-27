#pragma once

#include "coroutine.hpp"
#include <liburing.h>
#include <unordered_map>

namespace FishRPC{


  class event_loop{
  public:

    event_loop(const event_loop&) = delete;
    event_loop(event_loop&&) = delete;

    event_loop& operator=(const event_loop&) = delete;
    event_loop& operator=(event_loop&&) = delete;

    
    event_loop()
    {
      if(io_uring_queue_init( (1 << 10), &ring_, 0) < 0 ){
        throw std::runtime_error("io_uring_queue_init failed");
      }
    }

    ~event_loop()
    {
      io_uring_queue_exit(&ring_);
    }

    void run(task<void>&& main_task)
    {

      main_task.resume();
      if(main_task.is_done()){
        return;
      }

      
      while(true){ 
      
        if(main_task.is_done()){
          break;
        }

        handle_task();
        reap_task();
      }
      
    }
    

  private:
    io_uring ring_{};

    void handle_task(){
      for(auto& cur : tasks_){
        cur.second.resume();
      }
    }

    void reap_task()
    {
      for(auto it = tasks_.begin(); it != tasks_.end(); ){

        if(it->second.is_done() ){
          it = tasks_.erase(it);
        }
        else {
          ++it;
        }

      }

    }


    void add_task(task<void> task){
      tasks_ [task.handle.address()] = std::move(task);
    }


    std::unordered_map<void*, task<void>> tasks_;
  
  };


}