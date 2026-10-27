#include <cassert>
#include <iostream>
#include <type_traits>
#include <utility>
#include "event_loop.hpp"

int counter = 0;

// 最简协程：执行一次自增
FishRPC::task<void> do_work() {
  counter++;
  co_return;
}

// 带参数的计算协程：求 1..n 之和，结果写入 out 参数
FishRPC::task<void> accumulate(int n, int* sum) {
  for (int i = 1; i <= n; ++i) {
    *sum += i;
  }
  co_return;
}

// 包含 co_await 的协程（suspend_never 不会真正挂起）
FishRPC::task<void> with_co_await(int* n) {
  (*n)++;
  co_await std::suspend_never{};
  (*n)++;
  co_return;
}

// 多次 co_await 的协程
FishRPC::task<void> many_co_await(int* n) {
  for (int i = 0; i < 10; ++i) {
    co_await std::suspend_never{};
    (*n)++;
  }
  co_return;
}

// 1. 基本：run_once 把协程驱动到完成
void test_basic() {
  FishRPC::event_loop loop;
  counter = 0;
  loop.run_once(do_work());
  assert(counter == 1);
}

// 2. 同一个 loop 可反复使用
void test_loop_reuse() {
  FishRPC::event_loop loop;
  counter = 0;
  for (int i = 0; i < 1000; ++i) {
    loop.run_once(do_work());
  }
  assert(counter == 1000);
}

// 3. 带参数的协程
void test_parameter_task() {
  FishRPC::event_loop loop;
  int sum = 0;
  loop.run_once(accumulate(100, &sum));
  assert(sum == 5050);
}

// 4. 协程体内的 co_await（suspend_never）
void test_co_await() {
  FishRPC::event_loop loop;
  int n = 0;
  loop.run_once(with_co_await(&n));
  assert(n == 2);
}

// 5. 多次 co_await
void test_many_co_await() {
  FishRPC::event_loop loop;
  int n = 0;
  loop.run_once(many_co_await(&n));
  assert(n == 10);
}

// 6. 多个 loop 同时存在，互不影响
void test_multiple_loops() {
  FishRPC::event_loop loop1;
  FishRPC::event_loop loop2;

  counter = 0;
  int n = 0;

  loop1.run_once(do_work());
  loop2.run_once(with_co_await(&n));

  assert(counter == 1);
  assert(n == 2);
}

// 7. 创建任务时协程体不执行，run_once 后才完成
void test_lazy_start_and_done() {
  FishRPC::event_loop loop;

  counter = 0;
  auto t = do_work();
  assert(counter == 0);   // initial_suspend 挂起，body 未执行
  assert(!t.is_done());

  loop.run_once(std::move(t));
  assert(counter == 1);
  assert(t.is_done());    // final_suspend 处挂起，视为完成
}

// 8. event_loop 不可拷贝、不可移动（编译期检查）
void test_non_copyable() {
  static_assert(!std::is_copy_constructible_v<FishRPC::event_loop>);
  static_assert(!std::is_copy_assignable_v<FishRPC::event_loop>);
  static_assert(!std::is_move_constructible_v<FishRPC::event_loop>);
  static_assert(!std::is_move_assignable_v<FishRPC::event_loop>);
}

int main() {
  test_basic();
  test_loop_reuse();
  test_parameter_task();
  test_co_await();
  test_many_co_await();
  test_multiple_loops();
  test_lazy_start_and_done();
  test_non_copyable();

  std::cout << "event_loop tests passed" << std::endl;
  return 0;
}
