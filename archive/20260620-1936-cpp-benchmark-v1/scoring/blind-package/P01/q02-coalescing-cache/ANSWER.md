# 作答说明

## 修改摘要

### 设计

使用 `shared_ptr<State>`（PIMPL）持有所有可变状态：

```
State {
  mtx          — 保护 cache、in_flight、lru
  capacity     — 构造函数传入
  now          — Clock（只在锁外调用）
  cache        — unordered_map<string, pair<CacheValue, list-iterator>>
  lru          — list<string>（front = 最近使用）
  in_flight    — unordered_map<string, shared_ptr<Flight>>
}
```

### 状态机

```
get_or_load(key, ttl, loader)
  ├─ [递归加载检测] 若 key 在 thread_local loading_keys 中 → recursive_load
  ├─ [检查 ready cache] 持锁 → 存在且未过期 → touch(LRU) + 返回
  ├─ [检查 in_flight]  持锁 → 存在 → 在 flight->cv 上等待 → 返回结果
  └─ [创建新 flight]
       ├─ 持锁 → in_flight[key] = flight
       ├─ loading_keys.insert(key)
       ├─ loader(key)  ← 锁外执行
       ├─ loading_keys.erase(key)
       ├─ now()        ← 锁外执行
       └─ 持锁 →
            ├─ 检查 flight->should_cache & result.ok & ttl>0 → 写 cache（LRU）
            ├─ flight->done = true（flight->mtx 下）
            ├─ flight->cv.notify_all()
            └─ in_flight.erase(key)
```

### 锁边界

- **loader 和 Clock 始终在 `mtx` 外执行**：Clock 在 cache 检查和插入时分别获取两次（锁外），loader 全程在锁外。
- **flight 结果同步**：`done` 和 `result` 在 `flight->mtx` 下读写。找到 flight 的调用者等待 `cv` 并在同一个 `flight->mtx` 下读取结果。flight 从 `in_flight` 删除前，owner 已写入结果并通知所有等待者——任何找到 flight 的调用者通过同一 flight mutex 观察到完整结果。
- **callable 生命周期**：loader 是 caller 传入的引用，不涉及内部持有。

### invalidate 竞态处理

- `invalidate(key)`: 持锁 → 从 cache 移除（若有），若 key 在 `in_flight` 中，设 `flight->should_cache = false`。该 flight 的 loader 不受影响，等待者仍能拿到结果，但结果不会重新写入 ready cache。之后对该 key 的调用会触发新加载。
- 若 loader 在 `invalidate` 调用时刚完成但未写 cache：owner 持锁后检查 `should_cache`，跳过 cache 写入。

### LRU 策略

- `capacity=0` 时不保留任何 ready 值。
- 访问命中时 `touch(key)` → 移到 list front。
- 插入新条目时若达到 capacity，淘汰 list back。
- `in_flight` 条目不计入 capacity。

### 等待者唤醒顺序

等待者在 flight 的 `condition_variable` 上等待，owner 完成时 `notify_all()` 唤醒所有等待者。由于结果是不可变的 `LoadResult`，唤醒顺序不影响结果正确性。

## 验证

```bash
cd q02-coalescing-cache && ./run_public_checks.sh
```

输出：
```
Running public_checks.cpp
Running clock_reentrancy_checks.cpp
```

两项均静默退出，通过。`public_checks` 验证 6 线程并发压测同一 key 仅执行一次 loader；`clock_reentrancy_checks` 验证 Clock 内部调用 `invalidate("unrelated")` 不会死锁。

## 剩余风险

- thread_local loading_keys 在 loader 抛异常时正确回滚。
- 未覆盖的场景：多重 key 交错并发加载、TTL 到期后 LRU 淘汰边缘、loader 抛出非 std::exception 类型。