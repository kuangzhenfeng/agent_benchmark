# Q1 答案说明

## 修复点

1. **字符串存储**：将 `Subscription::topic` 和 `QueuedEvent` 的 `topic`/`payload` 从 `std::string_view` 改为 `std::string`，避免临时字符串失效。

2. **回调锁边界**：`drain()` 在锁内复制订阅者列表和事件队列，然后在锁外执行回调，避免在用户代码期间持有锁。

3. **事件批次快照**：`drain()` 只处理调用开始时已在队列中的事件（`events.swap(pending_)`），回调中新发布的事件留给下一次 drain。

4. **异常处理**：单个回调抛异常不会阻止其余回调，`callback_errors` 计数异常次数，`delivered` 计数所有尝试调用次数。

5. **活跃 tick 更新**：回调后更新 `last_active_tick`，通过 ID 匹配写回主列表。

6. **expire_idle 32 位模运算**：使用 `now_tick - last_active_tick >= ttl` 判断过期，正确处理回绕。

## 处理顺序

- 先截取事件批次和订阅者快照
- 对每个事件，遍历订阅者快照
- 回调中抛异常不影响其他订阅者
- 回调中 unsubscribe 不影响当前批次（因为使用的是快照）
- 回调中 subscribe 不接收当前批次（新订阅者不在快照中）
