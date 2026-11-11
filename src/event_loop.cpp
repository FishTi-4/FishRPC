#include "event_loop.hpp"

#include <stdexcept>

namespace FishRPC{

  event_loop::event_loop()
  {
    if(io_uring_queue_init( (1 << 10), &ring_, 0) < 0 ){
      throw std::runtime_error("io_uring_queue_init failed");
    }
  }

  event_loop::~event_loop()
  {
    io_uring_queue_exit(&ring_);
  }

  void event_loop::run(task<void>&& main_task)
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

  void event_loop::add_task(task<void> task){
    
    auto& h = task.handle;
    
    task.resume();
    if(h.done())  {
      return;
    }

    tasks_ [task.handle.address()] = std::move(task);
  }

  io_uring& event_loop::get_ring(){
    return ring_;
  }

  void event_loop::handle_task(){


    // for(auto& cur : tasks_){
    //   cur.second.resume();
    // }
    
    io_uring_cqe* cqe;
    int cnt = 0;
    int head = 0;
    io_uring_submit_and_wait(&ring_, 1);
    io_uring_for_each_cqe(&ring_, head, cqe ){
      auto task = reinterpret_cast<awaitable_base*>(cqe->user_data);
      task->result = cqe->res;

      if(task->type == awaitable_base::Type::resume ){
        task->handle.resume();
      }

      ++ cnt;
    }

    io_uring_cq_advance (&ring_, cnt);
  }


  void event_loop::reap_task()
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

} // namespace FishRPC
