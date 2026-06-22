# 作答说明

## 修改摘要

- 将订阅 topic 和待投递事件都改为自有 `std::string` 存储，避免把来自临时 `std::string` 的 `std::string_view` 跨调用悬挂；回调拿到的 `Event` 视图只指向当前 `drain` 局部批次中的稳定字符串。
- 将回调封装到 `shared_ptr<CallbackSlot>` 中。`subscribe` 在加锁前构造 slot，`unsubscribe/expire_idle` 在锁内只搬运 `shared_ptr`，真正释放最后一个 owner 发生在锁外，满足 callable 复制/移动/析构都不能卡在内部锁里的要求。
- `drain(now_tick)` 先在锁内把当时队列整体搬到局部批次，并记下批次开始时的订阅 ID 上界；之后每个事件再单独快照“该事件开始时仍订阅、且属于本批次前已存在的订阅者”。因此：
  - 回调中新发布的事件不会进入当前这次 `drain` 的批次。
  - 回调中新建订阅不会收到当前批次中任何事件。
  - 回调中取消订阅不会撤销它对“当前事件”的既有资格，但会阻止它接收后续事件。
- 回调执行全在锁外，异常逐个隔离；每次实际尝试调用都计入 `DispatchStats::delivered`，抛异常时额外累加 `callback_errors`，并且该事件不会因为异常残留到下一次 `drain`。
- `expire_idle` 改为基于 `uint32_t` 模减法判断经过时间，满足 32 位 tick 回绕和 `ttl == 0` 立即过期语义。

## 验证

- `cd q01-subscription-hub && ./run_public_checks.sh`
- `cd q01-subscription-hub && g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread -Iinclude src/subscription_hub.cpp tests/reentrancy_batch_checks.cpp -o /tmp/q01-extra && timeout 5s /tmp/q01-extra`
- 新增 `tests/reentrancy_batch_checks.cpp`，覆盖临时字符串、批次边界、回调内退订/新增订阅、异常统计和 `uint32_t` 回绕过期。

## 剩余风险

- 目前补充测试没有上 ThreadSanitizer；并发正确性主要依赖锁边界和批次快照设计推导，以及公开检查/附加语义测试。
