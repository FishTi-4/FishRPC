# FishRPC

基于 **C++20 协程** 与 **io_uring** 的轻量级 RPC（远程过程调用）框架。

FishRPC 使用 C++20 协程编写异步网络代码，底层 I/O 完全交由 io_uring 完成，
配合 Protobuf 进行序列化，让你可以用近乎同步的写法获得异步的性能。

## ✨ 特性

- **C++20 协程**：自研 `task<T>` 协程类型，`co_await` 风格的同步式异步编程
- **io_uring 驱动**：accept / read / write / connect 全部基于 io_uring 完成，无传统线程阻塞
- **Protobuf 序列化**：接口协议由 `.proto` 文件定义，跨语言兼容
- **多线程服务端**：基于 `SO_REUSEPORT`，每个线程拥有独立事件循环，天然多核扩展
- **注册式 API**：服务端一行注册方法，客户端一次 `call` 即可发起远程调用
- **轻量无依赖框架**：除 liburing 与 Protobuf 外无第三方库

## 🏗 架构

```
┌─────────────────────────────┐
│          RpcClient          │  ── call<REQ, RESP>(method, req)
├─────────────────────────────┤
│           event_loop        │  ── 协程调度 + io_uring 完成队列轮询
├─────────────────────────────┤
│      awaitable / task<T>    │  ── io_uring 异步操作的协程封装
├─────────────────────────────┤
│    protocol (帧协议读写)      │  ── 4 字节大端长度前缀 + Protobuf Frame
└─────────────────────────────┘
              │
        ──────▼──────  TCP  ───────
              │
┌─────────────────────────────┐
│          RpcServer          │  ── register_method<REQ, RESP>(name, fn)
├─────────────────────────────┤
│   每个线程独立 event_loop     │  ── SO_REUSEPORT 多线程监听
├─────────────────────────────┤
│    frame_worker（每连接）     │  ── 读请求 → 分发方法 → 写响应
└─────────────────────────────┘
```

## 📦 依赖

| 依赖 | 版本要求 | 说明 |
| ---- | -------- | ---- |
| Linux 内核 | ≥ 5.6 | io_uring 支持 |
| [liburing](https://github.com/axboe/liburing) | 任意较新版本 | io_uring 用户态库 |
| [Protobuf](https://github.com/protocolbuffers/protobuf) | 3.x | 序列化与代码生成 |
| CMake | ≥ 3.20 | 构建系统 |
| GCC / Clang | GCC ≥ 11 / Clang ≥ 14 | 需要完整的 C++20 协程支持 |

Ubuntu / Debian 安装依赖：

```bash
sudo apt install liburing-dev protobuf-compiler libprotobuf-dev cmake g++
```

## 🚀 快速开始

### 构建

```bash
git clone https://github.com/FishTi-4/FishRPC.git
cd FishRPC
cmake -B build
cmake --build build -j
```

### 运行示例

先启动服务端（监听 `8080` 端口）：

```bash
./build/example_server
```

再开一个终端运行客户端：

```bash
./build/example_client
# 输出: 3 + 4 = 7
```

### 运行测试

```bash
ctest --test-dir build --output-on-failure
```

## 💻 使用方式

### 1. 定义接口协议

用 Protobuf 定义你的请求 / 响应结构，例如 `Task.proto`：

```protobuf
syntax = "proto3";

package Task_protocol;

message ADD_REQ {
  int32 a = 1;
  int32 b = 2;
}

message ADD_RESP {
  int32 result = 1;
}
```

### 2. 编写服务端

```cpp
#include "server.hpp"
#include "Task.pb.h"

FishRPC::RpcServer server;

void reg() {
  server.register_method<Task_protocol::ADD_REQ, Task_protocol::ADD_RESP>(
      "add",
      [](Task_protocol::ADD_REQ req) {
        Task_protocol::ADD_RESP resp;
        resp.set_result(req.a() + req.b());
        return resp;
      });
}

int main() {
  reg();
  server.start("8080", /*thread_num=*/4);  // 阻塞，4 个线程监听同一端口
}
```

`register_method<REQ, RESP>(name, fn)` 的函数签名要求为
`RESP fn(REQ req)`，框架自动完成参数反序列化与结果序列化。

### 3. 编写客户端

```cpp
#include <iostream>
#include "client.hpp"
#include "Task.pb.h"

using namespace FishRPC;
using namespace Task_protocol;

task<void> demo(RpcClient& client) {
  if (!(co_await client.connect("127.0.0.1", "8080")))
    throw std::runtime_error("connect failed");

  ADD_REQ req;
  req.set_a(3);
  req.set_b(4);

  ADD_RESP resp = co_await client.call<ADD_REQ, ADD_RESP>("add", req);
  std::cout << "3 + 4 = " << resp.result() << "\n";
}

int main() {
  RpcClient client;
  client.run(demo(client));  // 阻塞，直到 demo 协程结束
}
```

在协程里 `co_await client.call<...>(...)` 会挂起当前协程，直到远端响应返回，
期间事件循环可以继续调度其他任务——同步的写法，异步的性能。

## 📡 协议设计

TCP 帧格式（见 `proto/FishRPC.proto`）：

```
┌──────────────────┬─────────────────────────────┐
│  4 字节长度 (大端)  │  Protobuf 序列化的 Frame 负载  │
└──────────────────┴─────────────────────────────┘
```

```protobuf
message Frame {
  oneof package_type {
    Req  request  = 1;  // 请求: { id, method, params }
    Resp response = 2;  // 响应: { id, error, error_message, result }
  }
}
```

- `id`：请求唯一标识，用于客户端将响应与挂起的协程匹配
- `params` / `result`：业务层请求与响应的 Protobuf 序列化字节流
- `error`：服务端执行失败（如方法未注册）时置位

## 📂 项目结构

```
FishRPC/
├── include/              # 头文件（库对外的全部接口）
│   ├── coroutine.hpp     #   task<T> 协程类型
│   ├── awaitable.hpp     #   io_uring 异步操作的 awaitable 封装
│   ├── event_loop.hpp    #   事件循环与协程调度
│   ├── protocol.hpp      #   帧协议读写
│   ├── server.hpp        #   RpcServer
│   └── client.hpp        #   RpcClient
├── src/                  # 实现文件
├── proto/                # Protobuf 协议定义
│   ├── FishRPC.proto     #   框架内部帧协议
│   └── Task.proto        #   示例业务协议
├── examples/             # 示例程序（server / client）
├── test/                 # 单元测试
└── CMakeLists.txt
```

## ⚠️ 已知局限

- 仅支持 Linux（依赖 io_uring）
- 暂无连接池、负载均衡、服务注册发现等企业级能力
- 客户端为单连接模型，暂不支持并发多路复用同一连接上的多个请求等待

以上均为未来规划方向，欢迎贡献。

## 📄 开源协议

本项目采用 [MIT License](./LICENSE) 开源。

## 🤝 参与贡献

欢迎提交 Issue 与 Pull Request！请先阅读 [贡献指南](./CONTRIBUTING.md)。
