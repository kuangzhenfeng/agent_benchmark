# 作答说明

## 修改摘要

### 缺陷定位与修复策略

1. **string_view 所有权（payload/topic 损坏）**：`Subscription::topic`、`QueuedEvent::topic` 和 `QueuedEvent::payload` 从 `std::string_view` 改为 `std::string`，确保所有持久数据拥有自己的存储。回调中 `Event` 的视图指向 drain 局部 `events` deque 中的字符串，仅在回调返回前有效。

2. **drain 期间的锁重入与死锁**：drain 不再在持有 mutex 的情况下调用回调。改为先获取锁、将 pending 事件 swap 到局部 deque 并快照订阅（拷贝 callback），然后释放锁。回调在无锁环境下执行。`subscribe`、`unsubscribe`、`publish`、`drain`、`expire_idle` 可在回调中安全重入。

3. **drain 只处理批次快照事件**：通过 `events.swap(pending_)` 将调用时已在队列中的事件移到局部变量。回调中新发布的事件写入新的空 pending，留给下一次 drain。

4. **回调中取消订阅阻止后续事件**：引入 `active_ids_`（`unordered_set`）。`unsubscribe` 在持锁时同时从 `subscriptions_` 和 `active_ids_` 中移除。drain 每次调用回调前重新检查 `active_ids_`，已取消的订阅者被跳过。新订阅不进入本次快照，因此不接收已截取事件。

5. **回调中新增订阅不接收当前批次事件**：订阅快照在 drain 开始时就已固定（拷贝），新订阅不在快照中。

6. **回调异常隔离**：try/catch 包裹每个回调；异常时仍递增 `delivered` 并额外递增 `callback_errors`；后续回调继续执行。该事件视为已处理（从 pending 移除），不会在下次 drain 重复投递。

7. **32 位时钟回绕**：`expire_idle` 使用 `uint32_t elapsed = now_tick - last_active_tick`（自然回绕），然后 `elapsed >= ttl`。TTL=0 时 `elapsed >= 0` 恒成立，即时过期。条件 `ttl < 2^31` 保证语义正确。

### 事件批次快照、取消订阅和异常的处理顺序

1. 加锁，swap events，快照 subscriptions 为 sub_snaps（拷贝），解锁。
2. 对每个 event E，遍历 sub_snaps：先检查 `active_ids_`（加锁检查后解锁），若已取消则跳过；否则检查 topic 匹配；匹配则 try/catch 调用 callback；无论成功或异常都递增 delivered，异常额外递增 callback_errors。
3. drain 结束后，加锁批量更新 last_active_tick。

## 验证

运行环境：g++ C++17，`./run_public_checks.sh` 通过。

新增测试覆盖：
- **临时 string_view 所有权**：订阅和发布使用会被 clear 的 std::string，验证数据仍然正确。
- **回调重入 publish + drain**：验证回调中 publish 的事件不被当前 drain 处理，但可由后续 drain 处理。
- **回调中 unsubscribe**：验证被取消的订阅者不接收后续事件。
- **回调中 subscribe**：验证新订阅者不接收当前批次事件。
- **回调异常隔离**：中间订阅者抛异常，前后订阅者都正常收到事件，stats 计数正确。
- **expire_ttl 正常未过期**：刚 drain（更新 tick）后 expire_idle 不应过期。
- **expire_idle uint32 回绕**：`last_active_tick=0xFFFFFF00, now_tick=0x100, ttl=0x200` 回绕后 elapsed=0x200，正确过期。
- **TTL=0 立即过期**：subscribe 后立刻 expire_idle(ttl=0) 即过期。
- **多线程并发 drain**：两个线程同时 drain，事件总计正确。

## 剩余风险

- 多线程同时 drain 时对同一订阅的 `last_active_tick` 更新由后写入的线程决定（最终值略不精确），但不影响正确性。
- `active_ids_` 线性查找（`unordered_set`）在大规模订阅下性能可能受限，可考虑更高效的 ID 索引，但语义正确。
