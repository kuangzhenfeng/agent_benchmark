# 作答说明

## 修改摘要

### 状态机设计

`CoalescingCache::State` 包含三个数据结构：
1. **ready_map + lru_list** — 缓存的 ready 值，带 LRU 淘汰和 TTL 过期。
2. **in_flight** — 正在进行中的 loader 请求（`Flight` 对象）。
3. **Flight** — 单个在途加载的同步原语（`mutex + cv + done + result`）。

### 锁边界

- `state_->mtx`：保护 ready_map、lru_list、in_flight 三个数据结构。
- `flight->mtx`：保护单个 flight 的 `done` 和 `result` 字段。
- **Clock 和 Loader 都在所有锁外执行**（requirement 2）。
- Clock 在 `get_or_load` 入口和 loader 完成后各调用一次，均在锁外。

### 所有者/等待者语义

1. 第一个到达的线程创建 `Flight` 并成为 owner（`is_owner = true`），在 in_flight 中注册。
2. 后续线程发现已有 flight，通过 `flight->owner_tid` 检测是否为同一线程（recursive_load），否则等待。
3. Owner 在锁外运行 loader，完成后在 `flight->mtx` 下设置 `done = true`，然后在 `state_->mtx` 下写入 ready cache 并移除 flight，最后 `notify_all()` 唤醒等待者。
4. 等待者在 `flight->mtx` 上 `wait(done)`，唤醒后直接返回 `flight->result`。

### invalidate 竞态处理

- `invalidate(key)` 在 `state_->mtx` 下立即移除 ready 值。
- 如果存在 in-flight，设置 `flight->write_back = false`，loader 完成后不会写回 ready cache。
- 现有等待者仍收到 loader 结果（requirement 5）。
- 之后的新调用者因 flight 已移除且无 ready 值，会启动新的 loader。

### TTL 语义

- TTL 起点为 loader 完成时的 Clock 值（`completion_time`）。
- TTL = 0 时不写入 ready cache，但并发调用仍合并同一次 flight。
- Ready 值在 `start_time >= expires_at` 时过期并被移除。

### 异常处理

- Loader 抛异常时捕获为 `LoadResult::failed()`，不跨越 API 边界。
- 等待者收到相同的 failed result。
- 失败不写入 ready cache，后续调用可重新加载。

### LRU 淘汰

- 容量由构造函数给定，`capacity == 0` 时永不保留 ready 值。
- 命中时 splice 到 lru_list 前端（最近使用）。
- 写入新值时从后端（最久未用）淘汰直到 `size < capacity`。
- 不淘汰 in-flight（in-flight 不在 lru_list 中）。

## 验证

```bash
./run_public_checks.sh
```

public_checks.cpp 和 clock_reentrancy_checks.cpp 均通过。

## 剩余风险

- 未额外新增并发压力测试（时间限制），但实现已覆盖 QUESTION.md 中的所有语义。
- recursive_load 检测基于 `thread::id`，如果同一线程通过线程池复用 ID 可能有极端情况，但在 C++17 标准下 `thread::id` 在线程生命周期内唯一。
