# Q3 ANSWER

## 修复概要

重构 `RoutingConfig`，解决 current() 悬空引用、观察者锁下调用死锁、观察者异常阻断和跨实例引用生命周期等问题。

## 关键设计

### 发布点
- `reload` 在 state mutex 下完成全部校验和新快照的创建/发布
- `current_snapshot_` 存储为 `shared_ptr<const Config>`，reload 成功时原子替换
- 新 Config 版本号在锁内计算为 `old_version + 1`

### current() 的引用保活机制
- 返回的 `const Config&` 指向 `shared_ptr<const Config>` 所持有的不可变 Config 对象
- 每个线程在 `thread_pins_`（通过 `pin_mutex_` 保护）中有独立的 `shared_ptr<const Config>` 槽位
- `current()` 在 state mutex 下获取最新快照 shared_ptr，在 pin_mutex_ 下写入当前线程的槽位
- 引用在当前线程再次调用同一实例的 `current()` 前持续有效
- 不同 `RoutingConfig` 实例的 `thread_pins_` 互不影响（per-instance map）
- reload 不触碰 `thread_pins_`，旧快照通过引用计数保活

### 观察者快照时刻与异常隔离
- 内部用 `shared_ptr<Observer>` 存储观察者（避免拷贝 `std::function` 触发用户代码）
- `subscribe`：锁外 `make_shared<Observer>`，锁内只移动 shared_ptr
- `unsubscribe`：锁内移出 shared_ptr，锁外析构
- `reload` 中在 state mutex 下拷贝观察者 shared_ptr 列表（仅增加引用计数），释放锁后逐个调用
- 观察者异常被 try/catch 捕获，`observer_errors_` 在 state mutex 下累加
- 观察者回调在锁外执行，可安全重入 `subscribe`、`unsubscribe`、`reload`、`snapshot`
- 回调中新建的订阅不影响当前通知批次，取消订阅只影响后续 reload

### find_target
- 基于 `snapshot()`（shared_ptr 拷贝）在锁外执行最长前缀匹配
- 保证查询基于一致的不可变快照

## 验证

```
bash run_public_checks.sh
# public_checks + cross_instance_current_checks（ASAN）+ observer_copy_reentrancy_checks 均通过
```

## 已知风险

- `thread_pins_` 中的 shared_ptr 在 RoutingConfig 析构时释放，如果外部仍持有 `current()` 返回的引用会导致 UAF（但这是合理的使用约束）
- `pin_mutex_` 在高并发 `current()` 调用时是串行化点，但锁持有时间极短
