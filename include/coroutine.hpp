#pragma once

#include <coroutine>
#include <exception>
#include <iostream>
#include <utility>

namespace FishRpc{

  struct taskBase{

  public:
  taskBase() = default;
  ~taskBase() = default;
    
    taskBase(const taskBase&) = delete;
    taskBase& operator=(const taskBase&) = delete;
    
    taskBase(taskBase&&) = default ;
    taskBase& operator=(taskBase&&) = default;

    // std::coroutine_handle<> handle;
  };

  template<typename T>
  struct task: public taskBase{ 
    
    struct promise_type{ 
      
      T value;

      task get_return_object() noexcept{
        return task{std::coroutine_handle<promise_type>::from_promise(*this)};
      }

      std::suspend_always initial_suspend() noexcept{
        return {};
      }

      std::suspend_always final_suspend() noexcept{
        return {};
      }

      void return_value(T value){
        this->value = value;
      }

      void unhandled_exception() noexcept{
        std::cout << "unhandled_exception" << std::endl;
        std::terminate();
      }

    };

    std::coroutine_handle<promise_type> handle;

    explicit task(std::coroutine_handle<promise_type> handle): handle(handle){
    }

    task() : handle(nullptr) {
    }

    task(task&& other) noexcept
      : taskBase(std::move(other)), handle(std::exchange(other.handle, nullptr)) {
    }

    task& operator=(task&& other) noexcept {
      if (this != &other) {
        if (handle) {
          handle.destroy();
        }
        taskBase::operator=(std::move(other));
        handle = std::exchange(other.handle, nullptr);
      }
      return *this;
    }

    ~task(){
      if(handle){
        handle.destroy();
      }
    }
  
    T get(){
      return handle.promise().value;
    }

    void resume(){
      handle.resume();
    }

    void destroy(){
      if(handle)
        handle.destroy();
    }

    bool is_done(){
      return !handle || handle.done();
    }

  };


  template<>
  struct task<void> : public taskBase{ 
      
    struct promise_type{ 

      task get_return_object() noexcept{
        return task{std::coroutine_handle<promise_type>::from_promise(*this)};
      }

      std::suspend_always initial_suspend() noexcept{
        return {};
      }

      std::suspend_always final_suspend() noexcept{
        return {};
      }

      void return_void(){}

      void unhandled_exception() noexcept{
        std::cout << "unhandled_exception" << std::endl;
        std::terminate();
      }

    };


    std::coroutine_handle<promise_type> handle;
    explicit task(std::coroutine_handle<promise_type> handle): handle(handle){
    }

    task() : handle(nullptr) {
    }

    task(task&& other) noexcept
      : taskBase(std::move(other)), handle(std::exchange(other.handle, nullptr)) {
    }

    task& operator=(task&& other) noexcept {
      if (this != &other) {
        if (handle) {
          handle.destroy();
        }
        taskBase::operator=(std::move(other));
        handle = std::exchange(other.handle, nullptr);
      }
      return *this;
    }

    ~task(){
      if(handle){
        handle.destroy();
      }
    }
    void resume(){
      handle.resume();
    }

    void destroy(){
      if(handle)
        handle.destroy();
    }

    bool is_done(){
      return !handle || handle.done();
    }

  };



 

} //namespace FishRPC
