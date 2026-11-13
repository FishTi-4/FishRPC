# 贡献指南

感谢你对 FishRPC 的关注！无论是报告 Bug、提出新特性、改进文档还是提交代码，你的每一份贡献都很有价值。

## 行为准则

- 尊重每一位贡献者，友善、专业地交流
- 讨论对事不对人
- 保持耐心，尤其是对新手提问

## 如何参与

### 报告 Bug

在提交 Issue 之前，请先搜索 [已有 Issue](https://github.com/FishTi-4/FishRPC/issues)，
确认没有重复。报告 Bug 时请尽量包含：

1. **环境信息**：Linux 内核版本（`uname -r`）、编译器版本（`g++ --version`）、liburing 与 Protobuf 版本
2. **复现步骤**：最小可复现的代码片段或命令序列
3. **实际行为与期望行为**：发生了什么，你期望发生什么
4. **日志 / 错误信息**：如有 coredump 或段错误，附上 `gdb` 回溯更好

### 提出新特性

描述你的想法时请说明：

- 要解决什么问题 / 使用场景是什么
- 期望的 API 形态（如果是接口变更）
- 是否愿意自己实现

### 提交代码

推荐的工作流程：

1. **Fork** 本仓库，并克隆到本地
2. 从最新的 `master` 分支创建功能分支：

   ```bash
   git checkout -b feat/my-feature
   ```

3. 开发并添加测试，确保改动没有破坏现有功能
4. 提交并推送你的分支，发起 **Pull Request**

## 开发环境

### 安装依赖（Ubuntu / Debian）

```bash
sudo apt install g++ cmake liburing-dev protobuf-compiler libprotobuf-dev
```

### 构建与测试

```bash
cmake -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

提交 PR 前，请确保所有测试通过。

## 代码规范

- 使用 **C++20** 标准，代码必须能通过 `-std=c++20` 编译
- 头文件使用 `#pragma once`
- 所有公开接口位于 `FishRPC` 命名空间内
- 头文件放入 `include/`，实现放入 `src/`（一个 `.hpp` 对应一个 `.cpp`）
- 命名风格与现有代码保持一致：
  - 类名：大驼峰（如 `RpcServer`、`RpcClient`）
  - 函数名 / 变量名：小写下划线（如 `read_frame`、`server_fd`）
  - 成员变量以下划线结尾（如 `method_`、`pending_`）
- 异步操作优先使用协程（`task<T>` + `co_await`）编写，禁止在事件循环中做阻塞调用
- 异常处理：不可恢复的错误使用 `throw std::runtime_error(...)` 抛出
- 注释：与现有代码一致使用中文即可

## Commit 规范

保持提交信息简洁明了，推荐格式：

```
<类型>: <简述>

[可选的详细说明]
```

类型示例：

| 类型 | 含义 |
| ---- | ---- |
| `feat` | 新功能 |
| `fix` | 修复 Bug |
| `docs` | 文档改动 |
| `test` | 测试相关 |
| `refactor` | 重构（不改变行为） |
| `chore` | 构建 / 杂项 |

示例：

```
feat: 客户端支持连接超时
fix: 修复大帧读取时缓冲越界
docs: 补充快速开始章节
```

每个提交只做一件事，避免把无关改动混在一起。

## 测试要求

- 新增功能请同时在 `test/` 目录添加对应的单元测试
- 在 `CMakeLists.txt` 中注册新的测试可执行文件，并加入 `test_name` 列表
- 测试需要能自动化运行（`ctest`），不依赖手动交互
- 涉及网络的功能测试，请使用回环地址并避免硬编码被占用的端口

## Pull Request 检查清单

提交 PR 前请确认：

- [ ] 代码通过编译且无警告
- [ ] 所有测试通过
- [ ] 新功能包含测试，API 变更已更新 README
- [ ] Commit 信息符合上文规范
- [ ] 已同步上游最新代码，解决冲突

## 许可证

所有贡献的代码均以项目的 [MIT License](./LICENSE) 发布。
提交 PR 即表示你同意在此协议下贡献你的代码。
