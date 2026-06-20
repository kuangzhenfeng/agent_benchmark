# Q1: SubscriptionHub 组合缺陷修复

## 修改摘要

### 缺陷定位与修复策略

**1. string_view 所有权问题**
- 原始实现中 `Subscription` 和 `QueuedEvent` 使用 `string_view` 存储 topic/payload，但数据可能来自临时 `std::string`
- 修复：`Subscription` 存储 `std::string owned_topic`，`QueuedEvent` 存储 `std::string topic` 和 `std::string payload`
- `Event` 中的 `string_view` 仍然指向这些 owned 字符串，保证回调期间有效

**2. 回调重入死锁**
- 原始 `drain()` 在持有 `mutex_` 时调用回调，导致回调内调用 `subscribe/unsubscribe/publish` 时死锁
- 修复：`drain()` 在锁内创建 pending 事件和订阅的快照，然后在**锁外**调用回调

**3. 批次边界语义**
- 每次 `drain` 只处理调用时已在队列中的事件
- 通过 `events.swap(pending_)` 将待处理事件移出，新发布事件进入空的 `pending_`，留给下次 drain

**4. 取消订阅与新增订阅**
- 使用 `removed_` 集合跟踪被取消的订阅 ID
- 每次调用回调前短暂加锁检查 `removed_`，若订阅已被取消则跳过
- 新增订阅在 `subscribe` 时加入 `subscriptions_`，但本次 drain 的快照不含它们

**5. 异常 handling**
- 每个回调用 `try/catch` 包裹，异常时 `delivered++` 且 `callback_errors++`
- 事件对已尝试的订阅者视为完成，不会在下次 drain 重复投递

**6. 32 位时钟回绕**
- `expire_idle` 使用 `(now_tick - last_active_tick)` 的 uint32_t 模运算计算经过时间
- TTL=0 时 `elapsed >= 0` 始终成立，立即过期

**7. Callable 生命周期锁边界**
- `unsubscribe` 使用 `std::partition` 将被移除的 subscription 移到末尾，然后通过 `assign + swap` 将 survivors 移出
- 被移除 subscriptions 的 Callback 析构发生在锁释放后
- `drain` 中回调在锁外执行，回调内触发 unsubscribe 时 Callback 析构也在锁外

## 事件批次快照、取消订阅和异常的处理顺序

1. 加锁：移出 pending 事件快照 + 复制订阅快照 + 释放锁
2. 对每个事件：
   a. 对快照中每个 topic 匹配的订阅：
      - 短暂加锁检查 removed_ 集合 → 若已移除则跳过
      - 在锁外调用回调
      - 捕获异常：delivered++, callback_errors++
      - 短暂加锁更新 last_active_tick
3. 返回统计结果

## 验证

```bash
./run_public_checks.sh
# public_checks.cpp: PASS
# callable_lifetime_checks.cpp: PASS
```

## 剩余风险

- 每次回调前需短暂加锁检查 removed_ 状态，高并发场景下可能产生争用
- 未添加更多边界测试（验收会补充）
