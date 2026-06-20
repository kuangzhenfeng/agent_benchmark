# 作答说明

## 修改摘要

### 状态机与锁边界

- **Flight 状态**：`done=false` → `done=true`，由 owner 在锁内完成。`done/result/invalidated` 均由 `state_->mutex` 保护（单一同步关系，满足规则7）。
- **锁边界**：loader 和 Clock 在 `state_->mutex` 之外执行；`invalidate` 和 `get_or_load` 的内部数据操作在锁内。
- **Flight 所有者**：`owner_thread = std::this_thread::get_id()`，loader 在该线程上执行。同 key 递归：检测 `flight->owner_thread == current_thread`，立即返回 `recursive_load`。
- **等待者**：在 `state_->mutex` 下 `cv.wait(ulock)`，唤醒后检查 `flight->done`（同一 mutex 保护的同步关系）。

### 失效与 LRU 策略

- `invalidate(key)`：锁内 `ready.erase(key)` + 若在途则 `flight->invalidated = true`。不取消 loader，但阻止其结果写入 ready cache。
- owner 完成时：`should_cache = ok && ttl > 0 && capacity > 0 && !flight->invalidated`。
- LRU 淘汰：`ready.size() >= capacity` 时遍历找最小 `lru_seq` 并移除。访问刷新 `lru_seq`。在途加载不占 ready 槽位，不会被淘汰。
- 容量 0：永不缓存，但并发仍合并同一次在途加载。

## 在途状态的所有者、等待者唤醒顺序和 invalidate 竞态处理

- **所有者**：创建 Flight 的线程，独占执行 loader。
- **等待者唤醒**：owner 完成后 `cv.notify_all()`，所有等待者依次重新获取 mutex，读取 `flight->result`。唤醒顺序取决于 OS mutex 调度，无特定保证。
- **invalidate 竞态**：invalidate 在锁内设置 `flight->invalidated=true`。owner 完成时也在锁内检查此标志。两操作在同一 mutex 下，无竞态。若 invalidate 发生在 owner 锁前，owner 看到 `invalidated=true`，不写 ready cache；若发生在 owner 锁后（不可能，因为 owner 持锁完成），不影响。

## 验证

```bash
./run_public_checks.sh
# public_checks.cpp ✓
# clock_reentrancy_checks.cpp ✓
```

新增边界测试 `edge_case_checks.cpp`：
- loader 抛异常
- loader 返回失败
- TTL 过期后重新加载
- invalidate 移除 ready + 阻止回写
- 递归同 key → recursive_load
- 容量 0 不缓存
- LRU 淘汰
- 并发 loader 异常

编译运行：`g++ -std=c++17 -pthread -Iinclude src/coalescing_cache.cpp tests/edge_case_checks.cpp && ./a.out` ✓

## 剩余风险

- LRU 淘汰为 O(n) 扫描，大规模缓存时性能不佳，但对正确性无影响。
- 等待者使用单一 `condition_variable`，可能收到无关 flight 的唤醒，但循环检查 `flight->done` 保证正确性。
