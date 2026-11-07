#include <cassert>
#include <iostream>
#include <string>
#include <utility>
#include "coroutine.hpp"

int counter = 0;

// task<void> 用例
FishRPC::task<void> bump() {
  counter++;
  co_return;
}

// co_return 之后的语句不应执行
FishRPC::task<void> bump_early() {
  counter++;
  co_return;
  counter++;
}

// task<int> 用例
FishRPC::task<int> add(int a, int b) {
  co_return a + b;
}

// 多分支 co_return：只返回实际执行到的那一支
FishRPC::task<int> pick(bool flag) {
  if (flag) co_return 1;
  co_return 2;
}

// task<bool> 用例：覆盖此前 get() 实例化失败的 bool 类型
FishRPC::task<bool> is_positive(int x) {
  co_return x > 0;
}

// 非平凡返回类型：验证 return_value 真正把值存进 promise
FishRPC::task<std::string> greet(std::string name) {
  co_return "hello, " + name;
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

  // 2. task<int>：co_return 的值可被 get() 取回
  {
    auto t = add(2, 3);
    assert(!t.is_done());
    t.resume();
    assert(t.is_done());
    assert(t.get() == 5);
  }

  // 3. 分支 co_return：返回实际执行到的分支
  {
    auto t = pick(true);
    t.resume();
    assert(t.get() == 1);

    auto f = pick(false);
    f.resume();
    assert(f.get() == 2);
  }

  // 4. task<bool>：覆盖此前实例化失败的 get()
  {
    auto t = is_positive(7);
    t.resume();
    assert(t.get() == true);

    auto u = is_positive(-1);
    u.resume();
    assert(u.get() == false);
  }

  // 5. 非平凡返回类型：验证 return_value 存的是值不是引用
  {
    auto t = greet("fish");
    t.resume();
    assert(t.get() == "hello, fish");
  }

  // 6. task<void>：co_return 之后不再执行
  {
    counter = 0;
    auto t = bump_early();
    t.resume();
    assert(counter == 1);
    assert(t.is_done());
  }

  // 7. 移动后 co_return 的结果仍可取；moved-from 不得二次 destroy
  {
    auto t = add(10, 20);
    auto moved = std::move(t);
    assert(!t.handle); // 移走后的 task 不再持有帧
    moved.resume();
    assert(moved.get() == 30);
  }

  // 8. 未 resume 直接析构：帧被销毁，return_value 从未调用
  {
    auto t = add(1, 1);
    assert(!t.is_done());
  } // 作用域结束，析构清理挂起的帧

  std::cout << "all tests passed" << std::endl;
  return 0;
}
