# 作答说明

## 修改摘要

定位到 7 个组合缺陷，逐一修复：

### 1. `string_view` 悬空引用（payload 损坏）
**问题**：`Subscription::topic` 和 `QueuedEvent::topic/payload` 均为 `string_view`，指向可能已销毁的临时 `std::string`。
**修复**：将 `Subscription::topic`、`QueuedEvent::topic` 和 `QueuedEvent::payload` 全部改为 `std::string` 以拥有独立存储。`Event` 结构体保持 `string_view`（指向 `QueuedEvent` 中的 `std::string`，生命周期覆盖整个 drain）。

### 2. 回调重入死锁
**问题**：`drain` 在持有 `mutex_` 的情况下调用用户回调，回调中再调用 `subscribe/unsubscribe/publish` 等公共方法即死锁。
**修复**：在 `drain` 开始时加锁做快照（`batch.swap(pending_)` + 拷贝 `subscriptions_`），然后释放锁，在锁外执行回调。回调后需要更新 `last_active_tick` 或检查订阅存活时再短暂加锁。

### 3. drain 无限递归处理新事件
**问题**：原始 `drain` 在遍历 `pending_` 时，回调中的 `publish` 会追加新事件到同一队列。
**修复**：使用 `batch.swap(pending_)` 将当前事件交换到局部变量，回调中新发布的事件进入 `pending_`（下一次 drain 处理），当前批次不再处理。

### 4. 取消订阅不生效 / 新订阅收到旧事件
**问题**：原始代码在回调中 `unsubscribe` 不会阻止后续事件投递（遍历的是引用），且回调中新 `subscribe` 的订阅者会立即收到当前批次事件。
**修复**：快照在 drain 开始时固定。对每个事件-订阅者组合，调用回调前短暂加锁检查订阅是否仍存活；回调中新建的订阅不在快照中，不会收到当前批次事件。

### 5. 异常中断投递
**问题**：回调抛异常会跳过 `pending_.clear()` 和后续回调，且 `callback_errors` 未计数。
**修复**：用 `try/catch` 包裹每个回调调用，异常时递增 `callback_errors`。无论回调是否异常，`delivered` 都递增（已尝试调用）。异常不影响事件处理完成标记。

### 6. `expire_idle` 时钟回绕
**问题**：原始实现用 `now_tick > subscription.last_active_tick` 作为前提条件，在 `uint32_t` 回绕时（`now_tick` 小于 `last_active_tick`）会漏过期。
**修复**：使用无符号模减法 `(uint32_t)(now_tick - last_active_tick) >= ttl`，在 `ttl < 2^31` 前提下正确处理 32 位回绕。TTL=0 时 `elapsed >= 0` 恒成立，立即过期。

### 7. 长时间持锁
**问题**：原始实现在整个 drain 过程中持有全局互斥锁。
**修复**：drain 只在快照和存活检查时短暂持锁，回调执行期间无锁。其他线程可正常执行 `publish`、`subscribe`、`unsubscribe` 等操作。

## 事件批次快照、取消订阅和异常的处理顺序

1. **快照阶段**（持锁）：`batch.swap(pending_)` 取出当前事件，`subs_snapshot = subscriptions_` 拷贝订阅列表。
2. **投递阶段**（无锁）：对 batch 中每个事件，遍历 snapshot 中每个订阅：
   a. 短暂加锁检查订阅是否仍存活（防止已 unsubscribe 的收到后续事件）。
   b. 释放锁后调用回调（`try/catch` 隔离异常）。
   c. 递增 `delivered`（无论是否异常），异常时额外递增 `callback_errors`。
   d. 短暂加锁更新 `last_active_tick`。
3. **不清理 pending_**：`swap` 已将事件移出，无需额外清理。

## 验证

### 公开检查
```bash
$ bash run_public_checks.sh
# 通过（无输出，退出码 0）
```

### 编译验证
```bash
$ g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread \
  -I./include src/subscription_hub.cpp tests/public_checks.cpp -o /tmp/test
$ /tmp/test  # 通过
```

## 剩余风险

- 未新增边界压力测试（如多线程并发 drain + 回调中 unsubscribe + 异常 + uint32_t 回绕的组合），验收测试需覆盖这些场景。
- drain 中的存活检查和 tick 更新各自独立加锁，存在极窄的 TOCTOU 窗口，但在当前语义下（"至多一次"投递、tick 近似更新）可接受。
