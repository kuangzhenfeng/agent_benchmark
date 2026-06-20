# 作答说明

## 修改摘要

### 缺陷定位

1. **string_view 悬空**：`Subscription` 和 `QueuedEvent` 保存 `string_view`，若传入临时 `std::string` 则视图立即失效。
2. **drain 中持锁调用回调**：`drain()` 在 `mutex_` 内直接遍历并调用 `subscription.callback(event)`，回调重入任何公共方法会死锁；同时异常会中断后续回调。
3. **无事件批次快照**：`drain` 在处理时不清空队列，回调中发布的事件可能在同一个 drain 中被处理，违反"只处理 drain 开始时的事件"规则。
4. **回调中取消订阅**：`unsubscribe` 移除 vector 元素，但 drain 的迭代器因元素删除而失效；回调中订阅无法从当前批次排除。
5. **callable 生命周期管理**：`unsubscribe` 和 `expire_idle` 在持锁时销毁 `std::function`，触发 `~ReenterOnDestroy` 重入死锁。
6. **32 位时钟回绕**：`expire_idle` 使用直接比较而非模运算。

### 修复策略

- **自有存储**：`Subscription::topic` 改为 `std::string`，`QueuedEvent::topic/payload` 改为 `std::string`，构造时从 `string_view` 拷贝。
- **事件批次快照 + 锁外回调**：`drain` 在持锁时拷贝 `pending_` 队列（清空）和 `subscriptions_` 列表（拷贝 callback）；解锁后逐个事件逐订阅处理。
- **活跃订阅跟踪**：新增 `active_ids_` (`unordered_set<SubscriptionId>`)，`subscribe` 插入，`unsubscribe`/`expire_idle` 移除。`drain` 在每次回调前检查 ID 是否仍在活跃集；回调中的新订阅因 ID 不在快照中，不会收到本次 drain 的事件。
- **异常隔离**：`drain` 中每个回调包裹 `try/catch`，异常不影响其他回调；delivered 计数所有尝试，`callback_errors` 单独计数异常。
- **callable 生命周期**：`unsubscribe` 和 `expire_idle` 将 callback `std::move` 出 vector 后释放锁，再让局部 `dying` 析构。
- **32 位模运算**：`expire_idle` 使用 `uint32_t elapsed = now_tick - last_active_tick`（无符号自动回绕），`elapsed >= ttl` 判定过期。

### 处理顺序（事件批次快照、取消订阅、异常）

```
drain(now_tick)
  ├── 持锁→快照 events (copy + clear pending_)
  │         快照 subs (copy subscriptions_)
  │      →解锁
  │
  └── for each event in events:
        for each sub in subs snapshot:
           ├── 持锁→检查 active_ids_(sub.id) →解锁
           │     └── 若不在 active_ids_ 中 → 跳过（回调中已取消订阅）
           │
           ├── callback(event)
           │     ├── 可重入 subscribe → 新 ID 不在 subs 快照中，不参与当前批次
           │     ├── 可重入 unsubscribe → active_ids_ 更新，后续 event 跳过
           │     ├── 可重入 publish → 事件入 pending_，留给下次 drain
           │     ├── 可重入 drain → 新 drain 独立快照
           │     ├── 可重入 expire_idle → 后续跳过已过期 ID
           │     └── 抛异常 → ++callback_errors，继续处理其他订阅者
           │
           └── 持锁→更新 last_active_tick（若订阅仍存在）→解锁
```

## 验证

```bash
cd q01-subscription-hub && ./run_public_checks.sh
```

输出：
```
Running public_checks.cpp
Running callable_lifetime_checks.cpp
```

两项均静默退出（无 assert 失败或超时），通过。

## 剩余风险

- `active_ids_` 集与 `subscriptions_` vector 在并发下保持原子性（同一 mutex 保护），但 `drain` 的快照与运行时的活跃检查之间可能存在极短窗口：订阅被移除后在 snapshot 中仍存在，但 `active_ids_` 检查会跳过它，不会调用 callback。
- `pinned_` 和 `observer_errors_` 在多线程场景下正确更新。