# 作答说明

## 修改摘要

### 定位的缺陷

1. **string_view 悬空**：`Subscription.topic` 和 `QueuedEvent.topic/payload` 存储 `std::string_view`，来自临时 `std::string` 时悬空。
2. **回调重入死锁**：`drain()` 持锁调用回调，回调重入 `subscribe/unsubscribe/publish/drain/expire_idle` 时死锁。
3. **批次语义违规**：回调中 `publish` 的事件在同一轮 drain 中被投递，违反"只处理调用开始时已存在的队列"语义。
4. **Callable 析构锁内执行**：`unsubscribe`/`expire_idle` 在锁内销毁 `Callback`，析构可能重入导致死锁。
5. **异常阻断其余回调**：单个回调异常终止所有后续回调投递。
6. **expire_idle 时钟回绕**：条件 `now_tick > last_active_tick && now_tick - last_active_tick >= ttl` 不支持 `uint32_t` 模运算回绕；TTL=0 未立即过期。

### 修复策略

- 私有字段 `string_view` → `std::string`（拥有存储），`Callback` → `std::shared_ptr<Callback>`（可安全锁外释放）。
- `drain`：锁内快照事件与 `max_original_id`，锁外逐事件、逐订阅者投递。每事件锁内重建 targets 列表（过滤 `id <= max_original_id` 且仍在 `subscriptions_` 中者），锁外调用回调，锁内更新 `last_active_tick`。
- 回调中 `unsubscribe`：从 `subscriptions_` 移除，后续事件不再投递。回调中 `subscribe`：ID > max_original_id，不接收本次 drain 事件。
- `unsubscribe`/`expire_idle`：锁内 `std::move(it->callback)` 到局部变量，erase 后解锁，局部变量析构在锁外。
- 异常隔离：`try/catch` 包裹每个回调，`++stats.delivered` 无论是否抛异常，`++stats.callback_errors` 仅计数异常。
- `expire_idle`：`static_cast<uint32_t>(now_tick - last_active_tick) >= ttl`，纯模运算；TTL=0 时 `elapsed >= 0` 恒真，立即过期。

## 事件批次快照、取消订阅和异常之间的处理顺序

1. drain 开始时锁内：移出 pending_ 到局部 `events`，计算 `max_original_id`（原订阅者的最大 ID）。
2. 解锁后逐事件投递：对每个事件，锁内扫描 `subscriptions_` 中 `id <= max_original_id` 且 topic 匹配的订阅者，复制 `shared_ptr<Callback>`，解锁后调用。
3. 回调中 unsubscribe：从 `subscriptions_` 移除该订阅者，下一个事件的 targets 不再包含它。
4. 回调中 subscribe：新 ID > max_original_id，所有事件投递时被过滤，不接收本轮任何事件。
5. 回调中 publish：新事件加入 `pending_`，留给下一次 drain。
6. 回调抛异常：`++delivered`，`++callback_errors`，不影响同事件其他订阅者或后续事件。

## 验证

```bash
./run_public_checks.sh
# public_checks.cpp ✓
# callable_lifetime_checks.cpp ✓
```

新增边界测试 `edge_case_checks.cpp`：
- 临时 string_view 生命周期
- 回调重入 unsubscribe/publish/subscribe
- 异常隔离
- uint32_t 时钟回绕 expire_idle
- TTL=0 立即过期
- 多线程并发 publish+drain

编译运行：`g++ -std=c++17 -pthread -Iinclude src/subscription_hub.cpp tests/edge_case_checks.cpp && ./a.out` ✓

## 剩余风险

- 高并发场景下 drain 每事件需多次短锁（获取 targets + 更新 tick），性能非最优但对正确性无影响。
- thread_local 未使用，不存在跨实例引用问题。
