# 作答说明

## 修改摘要

实现了 `CoalescingCache` 的并发请求合并缓存。核心设计：

### 在途状态的所有者

- `State::InFlight` 持有 `std::promise<LoadResult>` + `std::shared_future<LoadResult>`，以及 `loader_thread`（用于递归检测）和 `write_to_ready` 标志（由 `invalidate` 清零以阻止回写）。
- `State::inflight` 是 `unordered_map<key, shared_ptr<InFlight>>`。第一个到达的调用者创建 `InFlight` 并记录自己的线程 ID，成为 loader；后续调用者拷贝 `shared_future` 后释放 `state` 锁，在锁外等待。
- loader 执行完后在 `state` 锁下更新 ready cache（若允许）、从 `inflight` 移除条目；出锁后兑现 promise（唤醒等待者）。`shared_ptr` 局部变量保活 `InFlight`，确保 promise 在兑现前不被销毁，避免 `broken_promise`。

### loader 锁外执行与重入

- loader 在 `state` 锁外执行。重入不同 key：正常获取 `state` 锁→创建该 key 的 `InFlight`→执行内层 loader，不会死锁（外层 loader 不持锁）。
- 重入同一 key：入口处发现 `inflight[key].loader_thread == this_thread::get_id()`，立即返回 `LoadStatus::recursive_load`，不等待自己。

### TTL 与时钟

- 成功结果以 loader **完成后**调用的 `now()` 作为 TTL 起点，`expiry = completion_time + ttl`。
- ready 命中时比较 `now() < expiry`，过期则移除并重新加载。
- `ttl == 0`：不写 ready cache，但并发调用仍合并同一次在途加载（共享 future）。

### 失败与异常

- loader 返回 `loader_failed` → 等待者拿到同一失败结果，不写 ready。
- loader 抛异常 → `catch(...)` 转为 `LoadResult::failed`，不跨 API 边界，等待者拿到失败结果。
- 之后调用可重新加载。

### invalidate 竞态

- `invalidate(key)` 在 `state` 锁下：移除 ready 条目；若 key 在途，置 `write_to_ready = false`。
- loader 完成后检查 `write_to_ready`：被 invalidate 过则不回写 ready（但不取消 loader，结果仍兑现给 promise，现有/后续 joiner 共享同一结果）。
- 出锁后才 `promise.set_value`，避免在锁内唤醒所有等待者。

### LRU 淘汰

- ready cache 用 `std::list<std::string>`（front=MRU）+ `unordered_map<key, ReadyEntry>`，`ReadyEntry` 存 `value/expiry/list::iterator`。
- 命中时 erase+push_front 刷新位置；插入前 `evict_until_fit` 从 back 淘汰直到 `size < capacity`。
- 在途条目在 `inflight` map，不受 LRU 影响。
- `capacity == 0`：跳过所有 ready 写入。

### size()

仅返回 `ready.size()`，在 `state` 锁下读取，返回自包含值。

## 验证

- `./run_public_checks.sh`：通过（6 线程并发 miss 合并为 1 次 load）。
- 新增 `tests/edge_checks.cpp`，覆盖：
  - TTL 到期触发重新加载（`test_ttl_expiry_triggers_reload`）
  - TTL=0 不缓存但并发合并（`test_ttl_zero_no_cache_but_coalesces`）
  - loader 失败不缓存、允许重新加载（`test_loader_failure_not_cached_allows_reload`）
  - loader 异常被捕获、不跨边界（`test_loader_exception_caught_no_cross_boundary`）
  - 并发失败共享同一结果（`test_concurrent_failure_shared`）
  - LRU 淘汰 + 命中刷新位置（`test_lru_eviction`、`test_lru_access_refreshes_position`）
  - capacity=0 永不缓存（`test_capacity_zero_never_caches`）
  - 同 key 重入返回 recursive_load（`test_recursive_same_key`）
  - 不同 key 重入不死锁（`test_reentrant_different_key_no_deadlock`）
  - invalidate 移除 ready 值（`test_invalidate_removes_ready`）
  - invalidate 在途阻止回写（`test_invalidate_during_load_prevents_writeback`）
  - invalidate 后 joiner 仍共享在途结果（`test_invalidate_during_load_joiners_share_result`）
  - 多线程不同 key 并发不漏不重（`test_concurrent_distinct_keys`）
- 编译：`g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread -Iinclude src/coalescing_cache.cpp tests/edge_checks.cpp -o /tmp/q2_edge`
- 运行结果：全部断言通过（exit=0）。

## 剩余风险

- `now()` 回调在 `state` 锁内调用（用于 expiry 比较/记录完成时间）。若回调自身重入 `get_or_load` 会死锁；这是用户契约问题，题目未要求处理。
- `promise.set_value` 出锁后执行；在 loader 写 ready 与 set_value 之间的窗口内，新调用者已能命中 ready（因写回在锁内完成），不会触发重复 load。
- 极高并发下 `std::function` 拷贝与 `unordered_map` rehash 可能产生短暂延迟，但不影响正确性。
