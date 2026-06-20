# Q3：RoutingConfig 修改说明

## 修改内容

### 1. 安全发布
- 用 `std::shared_ptr<const Config> published_` 替代原来的 `Config current_`
- `reload` 先校验 candidate，通过后原子替换 `published_`
- `snapshot()` 返回 `published_` 的副本（shared_ptr 复制）

### 2. current() 的引用保活机制
- `current()` 返回 `const Config&`，但底层存储是 `shared_ptr<const Config>`
- 每个线程第一次调用 `current()` 时，在 `thread_pins_` 中记录该线程看到的快照
- 后续调用 `current()` 返回该 pinned 快照的引用
- 其他线程的 `current()` 调用不影响当前线程的 pinned 引用
- reload 不修改旧快照（创建新的 shared_ptr），旧快照因引用计数保持有效

### 3. current() 跨实例生命周期
- 每个 `RoutingConfig` 实例有独立的 `thread_pins_`
- 实例 A 的 `current()` 引用的快照由 A 的 `published_` 和 A 的 `thread_pins_` 共同保活
- 实例 B 的 reload 不影响实例 A 的快照生命周期

### 4. 观察者快照时刻与异常隔离
- `reload` 成功发布后，在释放 `mutex_` 前复制观察者列表到局部 `vector`
- 在锁外遍历通知，每个观察者可重入调用
- 观察者抛异常时，在锁下累加 `observer_errors_`，不阻止其余观察者
- 回调中取消订阅只影响后续 reload（当前通知用的是锁下复制的列表）
- 回调中新建订阅不接收当前通知

### 5. Observer callable 生命周期
- Observer 在锁下复制（push_back 到 vector）
- 在锁外的循环中调用，Observer 的析构发生在锁外

### 6. find_target 一致性
- `find_target` 先调用 `snapshot()` 获取一致快照
- 基于该快照执行最长前缀匹配

## 验证方式

```bash
cd q03-routing-config && bash run_public_checks.sh
```

输出：
```
Running public_checks.cpp
Running cross_instance_current_checks.cpp with AddressSanitizer
Running observer_copy_reentrancy_checks.cpp
```

三组检查均通过（exit 0），包括 AddressSanitizer 下无 use-after-free。

## 已知风险

- `thread_pins_` 随线程数增长，长期运行可能有内存累积。可考虑弱引用或定期清理。
- `current()` 的 per-thread pin 策略在同一线程长时间不调用 `current()` 的情况下，pin 的快照不会自动更新到最新版本（但仍有效）。
