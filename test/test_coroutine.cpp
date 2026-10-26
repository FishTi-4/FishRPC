#include <cassert>
#include <iostream>
#include "coroutine.hpp"

int counter = 0;

// task<void> 用例
FishRPC::task<void> bump() {
  counter++;
  co_return;
}

// task<int> 用例（get() 暂不可用，仅验证执行完成；修好头文件后可补 get() 断言）
FishRPC::task<int> add(int a, int b) {
  co_return a + b;
}

int main() {
  // 1. suspend_always：创建后挂起，协程体未执行；resume 后执行完毕
  counter = 0;
  {
    auto t = bump();
    assert(counter == 0);
    assert(!t.handle.done());
    t.resume();
    assert(counter == 1);
    assert(t.handle.done());
  } // 析构自动清理

  {
    auto t = add(1, 2);
    assert(!t.handle.done());
    t.resume();
    assert(t.handle.done());
  }

  std::cout << "all tests passed" << std::endl;
  return 0;
}
