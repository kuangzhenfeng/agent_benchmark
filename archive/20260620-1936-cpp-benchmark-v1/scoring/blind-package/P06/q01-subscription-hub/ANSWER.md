# Q1 ANSWER

## 修复概要

修复了 `SubscriptionHub` 中的多个组合缺陷：string_view 悬空、回调重入死锁、重复投递、32 位时钟回绕和 callable 析构重入。

## 关键改动

### 1. 自有存储（消除悬空引用）
- `Subscription::topic` 改为 `std::string`
- `QueuedEvent::topic/payload` 改为 `std::string`
- `subscribe` 和 `publish` 将 `string_view` 拷贝为自有字符串

### 2. shared_ptr<Callback>（锁外管理 callable 生命周期）
- 内部用 `shared_ptr<Callback>` 存储回调
- `subscribe`：在锁外 `make_shared`，锁内只移动 `shared_ptr`
- `unsubscribe`：锁内移出 `shared_ptr`，锁外析构
- `drain`：锁内拷贝 `shared_ptr`（仅增加引用计数，不触发用户代码），锁外调用回调和释放
- `expire_idle`：过期的 callback 的 `shared_ptr` 移出后在锁外释放

### 3. drain 批次快照语义
- drain 开始时：锁内 swap 出 `pending_` 为本地 `batch`，记录所有当前订阅者 ID 到 `initial_ids`
- 每个事件投递时：锁内取当前仍存在且属于 `initial_ids` 的匹配订阅者快照（`shared_ptr<Callback>`），锁外逐个调用
- 回调中新发布的订阅 ID 不在 `initial_ids` 中 -> 不接收本批次事件
- 回调中取消的订阅已从 `subscriptions_` 移除 -> 不接收后续事件
- 回调中抛异常不影响其他回调；`delivered` 统计所有尝试调用（含异常），`callback_errors` 单独计数

### 4. 32 位模运算 expire_idle
- `uint32_t elapsed = now_tick - last_active_tick`（无符号减法自然回绕）
- `elapsed >= ttl` 即过期；`ttl == 0` 时立即过期（`0 >= 0`）

## 事件批次快照、取消订阅和异常的处理顺序

1. drain 开始：锁内取 pending 快照 + 初始订阅者 ID 集合
2. 对批次中每个事件：
   a. 锁内取当前存活且属于初始 ID 集合的匹配订阅者（shared_ptr 拷贝）
   b. 锁外逐个调用回调，try/catch 捕获异常
   c. 锁内更新被调用者的 `last_active_tick`
3. 取消订阅在步骤 2a 之前生效（已从 subscriptions_ 移除的 ID 不在快照中）
4. 异常在步骤 2b 中被捕获，不中断其他回调

## 验证

```
bash run_public_checks.sh  # public_checks + callable_lifetime_checks 均通过
```

## 已知风险

- 高并发下每个事件的锁获取/释放次数较多（3 次锁操作 × 事件数）
- `initial_ids` 使用 `unordered_set` 查找，大批量订阅时有哈希开销
