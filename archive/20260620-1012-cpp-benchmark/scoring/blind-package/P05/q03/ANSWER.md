# Q3: RoutingConfig 安全发布重构

## 修改摘要

### 快照发布机制

**原子发布**
- `reload()` 先在锁外校验 candidate，失败则返回 false
- 校验通过后加锁，创建不可变 `shared_ptr<const Config>` 作为新快照，更新 `current_`，复制 observers 快照，然后释放锁
- 通知观察者在锁外进行，观察者抛异常不影响已发布配置，`observer_errors_` 累加

**版本严格加一**
- `candidate.version = current_->version + 1`（忽略 candidate 的原始 version）

### current() 的引用保活机制

使用 `thread_local unordered_map<const void*, shared_ptr<const Config>>` 为每个 RoutingConfig 实例在每个线程维护一个 pinned snapshot：
- 每次 `current()` 调用时更新对应实例的 pin 为最新 `current_`
- 引用的生命周期持续到：该线程对同一实例的下一次 `current()` 调用，或线程退出
- 调用另一实例的 `current()` 不影响前一实例的 pin（per-instance map key 为实例指针）
- reload 替换 `current_` 时，旧 config 仍被 pin 持有，不会被销毁

### 观察者快照时刻与异常隔离

- reload 在锁内复制 `observers_` map（存 `shared_ptr<Observer>`），不拷贝 Observer 本身
- 复制 shared_ptr 不会触发 ReenterOnCopy 等特殊 callable 的构造函数
- 观察者在锁外通知，可重入 subscribe/unsubscribe/reload/snapshot
- 异常通过 try/catch 捕获，不影响其他观察者，不回滚配置
- 回调中的 unsubscribe 只影响后续 reload（因为快照是 reload 开始时的）

### 回调 callable 的锁外析构

- `unsubscribe()` 在锁内找到 observer 并移出 shared_ptr，释放锁后 shared_ptr 析构（可能触发用户 callable 析构）

### find_target 一致性

- 先 `snapshot()` 获取不可变快照，基于该快照做最长前缀匹配

## 验证

```bash
./run_public_checks.sh
# public_checks.cpp: PASS
# cross_instance_current_checks.cpp (with ASAN): PASS
# observer_copy_reentrancy_checks.cpp: PASS
```

## 剩余风险

- `thread_local` map 会在 RoutingConfig 实例销毁后保留 stale entry，直到线程退出或该 slot 被覆写，但不影响正确性（shared_ptr 已确保 config 生命周期）
- 未添加并发 reload + observer 重入压力测试
