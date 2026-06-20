# Q1：SubscriptionHub 修改说明

## 修改内容

### 1. 字符串所有权修复
- `Subscription::topic` 从 `std::string_view` 改为 `std::string`，拥有自己的存储
- `QueuedEvent::topic` 和 `QueuedEvent::payload` 从 `std::string_view` 改为 `std::string`
- `subscribe` 和 `publish` 中用 `std::string(topic/payload)` 构造副本

### 2. 回调重入与锁边界
- `drain` 不再在持有 `mutex_` 的情况下调用回调。改为：
  1. 锁下快照 pending 事件和当前订阅者（含 callback 副本）
  2. 清空 pending
  3. 解锁后逐个调用回调
  4. 每次回调后锁下更新 `last_active_tick`
- `subscribe`、`unsubscribe` 等方法保持不变，因回调已在锁外执行，重入调用不会死锁

### 3. drain 批次快照语义
- `drain(now_tick)` 开始时将 `pending_` 整体 move 到局部 `DrainSnapshot`，然后清空 `pending_`
- 回调中新发布的事件留在 `pending_` 中，留给下一次 `drain`
- 为每个事件收集当前仍活跃的订阅者 ID（锁下检查），然后匹配快照中的回调进行调用
- 回调中取消订阅后，该订阅者不会收到当前事件之后的事件（因为活跃 ID 列表每次事件重新收集）
- 回调中新订阅不接收本次 drain 已截取的事件（因为快照只包含 drain 开始时的订阅者）

### 4. 异常处理
- 每个回调用 try/catch 包裹，异常时 `callback_errors++` 但不阻止其余回调
- `delivered` 统计所有尝试调用次数（包括抛异常的）

### 5. expire_idle 32 位模运算
- 使用 `now_tick - subscription.last_active_tick` 的无符号减法自动处理回绕
- TTL=0 时立即过期（直接返回 true）
- 判断条件：`elapsed >= ttl`

### 6. 事件批次快照、取消订阅和异常处理顺序
- drain 开始时快照 pending 并清空
- 对 pending 中的每个事件：
  - 锁下收集当前仍订阅该 topic 的 ID 列表
  - 遍历快照中的订阅者，若 ID 在活跃列表中则调用
  - 回调抛异常则计数但不影响其余回调
  - 回调结束后锁下更新该订阅者的 last_active_tick

## 验证方式

```bash
cd q01-subscription-hub && bash run_public_checks.sh
```

输出：
```
Running public_checks.cpp
Running callable_lifetime_checks.cpp
```

两组检查均通过（exit 0）。

## 已知风险

- drain 中为每个事件重新锁下收集活跃订阅者 ID，高并发下可能有锁竞争。可通过更细粒度的快照优化。
- 回调中 subscribe/unsubscribe/publish 会重新获取 mutex_，但由于回调本身不在锁下持有，不会死锁。
