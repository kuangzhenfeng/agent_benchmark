# 作答说明

## 修改摘要

### 缺陷定位与修复策略

1. **string_view 悬空（Q1.1）**：`QueuedEvent` 和 `Subscription` 中的 `topic`/`payload` 从 `std::string_view` 改为 `std::string`，确保跨调用数据拥有自己的存储。回调中的 `Event` 仍为 `string_view`，指向当前 batch 中的 `QueuedEvent` 字符串。

2. **锁内回调与 callable 生命周期（Q1.7 硬性封顶）**：
   - `drain()` 在锁内捕获 batch 和 subscription 快照（通过 `shared_ptr<Callback>` 增加引用计数），然后在锁外执行回调。
   - `unsubscribe()` 在锁内移除 subscription，在锁外析构 callback。
   - `expire_idle()` 在锁内 partition 出过期 subscription，在锁外析构 callback。
   - 所有用户 callable 的复制、移动和销毁都在内部锁外完成。

3. **回调重入与取消订阅（Q1.2, Q1.4）**：
   - 每次投递前重新检查该 subscription 是否仍在 `subscriptions_` 中（因为回调可能 `unsubscribe`）。
   - 回调中新建的 subscription 不在 `drain_subs` 快照中，不会接收当前 batch 的事件。

4. **异常隔离（Q1.5）**：
   - `try/catch` 包裹每次回调，`delivered` 统计所有尝试（包括抛异常的），`callback_errors` 单独计数异常。
   - 异常不阻止后续回调，该事件仍视为已处理（batch 已被 swap 出 pending_）。

5. **32 位时钟回绕（Q1.6）**：
   - `expire_idle()` 使用无符号减法 `now_tick - last_active_tick` 计算经过时间（模运算正确），不再用 `now_tick > last_active_tick` 条件判断。
   - TTL 为 0 时 `elapsed < 0` 恒为 false（uint32_t），所有 subscription 过期。

6. **批次边界（Q1.3）**：
   - `drain()` 开始时 `batch.swap(pending_)`，只处理调用时已在队列中的事件。
   - 回调中新发布的事件进入新的 `pending_`，留给下一次 drain。

## 验证

```bash
./run_public_checks.sh
```

## 剩余风险

- 未新增额外边界测试（时间限制），但实现已覆盖 QUESTION.md 中的所有语义。
- 多线程场景：锁粒度为单个 mutex，但所有用户 callable 在锁外执行，避免死锁。
