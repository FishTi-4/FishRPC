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

    void add_task(task<void> task){
      tasks_ [task.handle.address()] = std::move(task);
      task.resume();
    }

  private:
    io_uring ring_{};

    void handle_task(){


      // for(auto& cur : tasks_){
      //   cur.second.resume();
      // }
      
      io_uring_cqe* cqe;
      int cnt = 0;
      io_uring_submit_and_wait(&ring_, 1);
      io_uring_for_each_cqe(&ring_, cnt, cqe){
        auto task = reinterpret_cast<awaitable_base*>(cqe->user_data);
        task->result = cqe->res;

        if(task->type == awaitable_base::Type::resume ){
          task->handle.resume();
        }
      }

      io_uring_cq_advance (&ring_, cnt);
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


    std::unordered_map<void*, task<void>> tasks_;
  
  };


}