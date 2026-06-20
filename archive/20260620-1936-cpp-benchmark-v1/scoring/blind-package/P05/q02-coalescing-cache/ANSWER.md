# Q2：CoalescingCache 修改说明

## 修改内容

完整实现了 `CoalescingCache` 的并发请求合并缓存逻辑。

### 1. 并发 loader 合并
- `in_flight_` 表记录每个 key 的在途加载状态
- 同一 key 的第一个调用者成为 owner 执行 loader
- 其他并发调用者找到 in-flight 条目后，通过 `InFlightState` 的 `condition_variable` 等待
- Owner 完成后 `notify_all` 唤醒所有等待者

### 2. Clock 和 loader 的锁边界
- `Clock::now()` 在锁外调用（`get_or_load` 入口处）
- `loader(key)` 在锁外调用（owner 路径）
- `InFlightState` 有独立的 `mutex` 和 `cv`，不依赖主 `mutex_`

### 3. 递归加载检测
- `loading_keys_` 记录每线程正在加载的 key 栈
- loader 重入请求同一 key 时，检测到后立即返回 `LoadStatus::recursive_load`
- 递归检测在锁外完成（用 `mutex_` 短时间保护查找）

### 4. TTL 和 LRU
- 成功结果以 loader 完成时的 clock 作为 TTL 起点（在 loader 返回后调用 `now_()`）
- LRU 使用 `std::list` + `unordered_map` 实现，访问时 `splice` 刷新位置
- 容量满时淘汰 front（least recently used）
- TTL=0 时不缓存（不写入 ready cache），但在途合并仍然有效

### 5. loader 异常处理
- loader 抛异常时捕获为 `LoadResult::failed`
- 等待者收到相同失败结果，失败不进入 ready cache

### 6. invalidate 竞态处理
- `invalidate` 从 ready cache 移除条目
- 若 key 正在加载，标记 `FlightInfo::invalidated = true`
- Owner 完成时检查 invalidated 标记，若为 true 则不写回 ready cache
- 不取消在途 loader，现有等待者仍收到结果

### 7. 在途状态的所有者、等待者唤醒顺序和 invalidate 竞态
- Owner 在锁下设置 `InFlightState::done = true` 和 `result`，然后 `notify_all`
- 等待者在 `cv.wait` 返回后读取 `result`
- invalidate 标记设置后，Owner 在设置 done 前检查该标记
- flight 从 `in_flight_` 移除后，新调用者要么看到 ready 值，要么开始新 flight

## 验证方式

```bash
cd q02-coalescing-cache && bash run_public_checks.sh
```

输出：
```
Running public_checks.cpp
Running clock_reentrancy_checks.cpp
```

两组检查均通过（exit 0）。

## 已知风险

- 高并发下 `loading_keys_` 的锁竞争可能成为瓶颈。可改用 thread_local 变量避免锁。
- LRU 淘汰和 flight 完成的锁范围较大，可考虑读写锁优化。
