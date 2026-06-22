# 作答说明

## 修改摘要

- 将订阅 topic 和排队事件的 topic/payload 改为自有 `std::string`，因此临时
  `std::string` 不会造成悬空视图。
- 订阅 callback 由 `shared_ptr<Callback>` 持有；`subscribe` 在加锁前构造它，
  `unsubscribe` 和过期删除将最后一个所有者移到锁外销毁。drain 仅在锁内复制
  指针，不复制、移动或析构用户 callable。
- `drain` 先交换出调用开始时的队列，并记录当时的订阅 ID 上界。每个事件开始
  投递时单独快照匹配的 callback 指针。故回调新增的订阅不会收到本 batch，取消
  的订阅仍可完成已开始的当前事件、但不会收到后续事件；回调发布的事件留给下一
  次 drain（包括显式的重入 drain）。
- 回调在锁外逐个调用。异常被隔离，仍计入 `delivered`，并计入
  `callback_errors`；每个实际尝试之后才更新仍存在订阅的活跃 tick。过期使用
  `uint32_t` 减法，TTL 为零时立即过期。

## 验证

已执行并通过：

```bash
./q01-subscription-hub/run_public_checks.sh
```

新增 `tests/edge_semantics_checks.cpp`，覆盖临时字符串、batch 截取、当前事件的
取消语义、drain 内新增订阅、异常统计，以及 `uint32_t` 回绕过期。
另以 `clang++ -std=c++17 -Wall -Wextra -Wpedantic -pthread` 逐个编译并运行三项
检查，均通过。

## 剩余风险

公开检查和新增确定性边界检查均通过；未在本目录内运行 ThreadSanitizer 的长时间
随机并发压力测试。
