#pragma once

#include <coroutine>
#include <exception>
#include <iostream>
#include <utility>

namespace FishRPC{

  struct taskBase{

  public:
  
    std::coroutine_handle<> handle;

    taskBase() = default;
    ~taskBase() = default;
      
    

    taskBase(const taskBase&) = delete;
    taskBase& operator=(const taskBase&) = delete;
    
    taskBase(taskBase&&) = default ;
    taskBase& operator=(taskBase&&) = default;



    // std::coroutine_handle<> handle;
  };

  // ==================== taks<T> ====================
  template<typename T>
  struct task: public taskBase{ 
    
    struct promise_type{ 
      
      T value;

      std::coroutine_handle<> fa;

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

    // std::coroutine_handle<promise_type> handle;


    struct awaiter{

      std::coroutine_handle<promise_type> son_handle;

      bool await_ready() {
        return false;
      }

      std::coroutine_handle<promise_type> await_suspend(std::coroutine_handle<> handle) {
        son_handle.promise().fa = handle;
        return son_handle;
      }

      void await_resume(){
        son_handle.destroy();
      }

    };

    awaiter operator co_await(){
      awaiter temp_awaiter{  std::exchange(this -> handle, nullptr)};

      return temp_awaiter;
    }


    explicit task(std::coroutine_handle<promise_type> handle){
      taskBase::handle = handle;
    }

    task(){
      taskBase::handle = nullptr;
    }

    task(task&& other) noexcept
      : taskBase(std::move(other))
    {
      // taskBase::operator=(std::move(other));
      handle = std::exchange(other.handle, nullptr);
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
      return std::coroutine_handle<promise_type>::from_address(handle.address()).promise().value;
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

  };  // ==================== taks<T> ====================




  // template<> // ==================== taks<void> ====================
  // struct task<void> : public taskBase{ 
      
  //   struct promise_type{ 

  //     std::coroutine_handle<> fa;

  //     task get_return_object() noexcept{
  //       return task{std::coroutine_handle<promise_type>::from_promise(*this)};
  //     }

  //     std::suspend_always initial_suspend() noexcept{
  //       return {};
  //     }

  //     std::suspend_always final_suspend() noexcept{
  //       return {};
  //     }

  //     void return_void(){}

  //     void unhandled_exception() noexcept{
  //       std::cout << "unhandled_exception" << std::endl;
  //       std::terminate();
  //     }

  //   };


  //   // std::coroutine_handle<promise_type> handle;
  //   explicit task(std::coroutine_handle<promise_type> handle) {
  //     taskBase::handle = handle;
  //   }

  //   task(){
  //     taskBase::handle = nullptr;
  //   }

  //   task(task&& other) noexcept
  //     : taskBase(std::move(other)) 
  //   {
  //     // taskBase::operator=(std::move(other));
  //     handle = std::exchange(other.handle, nullptr);
  //   }

  //   task& operator=(task&& other) noexcept {
  //     if (this != &other) {
  //       if (handle) {
  //         handle.destroy();
  //       }
  //       taskBase::operator=(std::move(other));
  //       handle = std::exchange(other.handle, nullptr);
  //     }
  //     return *this;
  //   }

  //   ~task(){
  //     if(handle){
  //       handle.destroy();
  //     }
  //   }
  //   void resume(){
  //     handle.resume();
  //   }

  //   void destroy(){
  //     if(handle)
  //       handle.destroy();
  //   }

  //   bool is_done(){
  //     return !handle || handle.done();
  //   }


  template<>
  struct task<void>: public taskBase{ 
    
    struct promise_type{ 
      
      std::coroutine_handle<> fa;

      task get_return_object() noexcept{
        return task{std::coroutine_handle<promise_type>::from_promise(*this)};
      }

      std::suspend_always initial_suspend() noexcept{
        return {};
      }

      std::suspend_always final_suspend() noexcept{
        return {};
      }

      void unhandled_exception() noexcept{
        std::cout << "unhandled_exception" << std::endl;
        std::terminate();
      }

      void return_void(){}

    };

    // std::coroutine_handle<promise_type> handle;


    struct awaiter{

      std::coroutine_handle<promise_type> son_handle;

      bool await_ready() {
        return false;
      }

      std::coroutine_handle<promise_type> await_suspend(std::coroutine_handle<> handle) {
        son_handle.promise().fa = handle;
        return son_handle;
      }

      void await_resume(){
        son_handle.destroy();
      }

    };

    awaiter operator co_await(){
      awaiter temp_awaiter{ std::coroutine_handle<promise_type>::from_address(std::exchange(this -> handle, nullptr).address()) };
      
      return temp_awaiter;
    }


    explicit task(std::coroutine_handle<promise_type> handle){
      taskBase::handle = handle;
    }

    task(){
      taskBase::handle = nullptr;
    }

    task(task&& other) noexcept
      : taskBase(std::move(other))
    {
      // taskBase::operator=(std::move(other));
      handle = std::exchange(other.handle, nullptr);
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

  // };// ==================== taks<void> ====================



 

} //namespace FishRPC
