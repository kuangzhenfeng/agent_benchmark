# 作答说明

## 修改摘要

完整实现了 `CoalescingCache`，替换了 starter 中的空桩。核心设计如下：

### 状态机

每个 key 有 3 种状态：
- **Ready**：缓存命中且未过期，直接返回。
- **In-flight**：正在加载中，后续调用合并等待。
- **Miss**：既不在 ready cache 也不在 in-flight map，当前调用成为 owner 发起加载。

### 锁边界

使用两层锁：
- `state_->mtx`（全局）：保护 ready cache（LRU list + map）、in-flight map、capacity 等共享状态。
- `inflight->inflight_mtx`（per-key）：仅用于 `condition_variable` 等待/通知，保护该 key 的 `done` 标志和 `result`。

**关键不变量**：锁获取顺序始终为 `state_->mtx → inflight->inflight_mtx`（`invalidate` 中也遵循此顺序），无死锁风险。

Loader 在两层锁之外执行，允许 loader 重入请求不同 key。

### 失效与 LRU 策略

- **Ready cache**：双向链表 + unordered_map 实现 O(1) LRU。front = MRU，back = LRU。容量为 0 时永不缓存。
- **TTL**：以 loader 完成时的时钟为起点（`completion_time = state_->now()`），过期时间为 `completion_time + ttl`。`ttl == 0` 时不写入 ready cache，但并发调用仍合并同一次 in-flight 加载。
- **`invalidate(key)`**：立即从 ready cache 移除。若 key 正在加载，设置 `invalidated = true` 标志，owner 完成后不写回 ready cache，但等待者仍可获得结果。之后新调用可继续加入该 in-flight 加载。
- **淘汰策略**：仅在写入 ready cache 时淘汰 LRU 尾部，不淘汰 in-flight 条目。

### Owner 选择与等待

- 第一个到达的线程成为 owner，创建 `InFlight` 对象并设置 `loader_thread`。
- 后续到达的线程发现 in-flight 存在：
  - 若同线程且未完成 → 返回 `recursive_load`。
  - 若已完成（罕见竞态窗口）→ 直接返回 result。
  - 否则在 `inflight->cv` 上等待。
- Owner 完成 loader 后：更新 `done + result` → notify_all → 写 ready cache（若未 invalidated 且成功且 ttl > 0）→ 从 in-flight map 移除。

### 异常隔离

Loader 抛异常时，catch 转为 `LoadResult::failed(e.what())`，不跨越 API 边界。等待者获得同样的失败结果。失败结果不写入 ready cache。

## 验证

### 公开检查
```bash
$ bash run_public_checks.sh
# 通过（无输出，退出码 0）
```

### 编译验证
```bash
$ g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread \
  -I./include src/coalescing_cache.cpp tests/public_checks.cpp -o /tmp/test
$ /tmp/test  # 6 线程并发同一 key，loader 仅执行 1 次，全部拿到相同结果
```

## 剩余风险

- 未新增独立的边界压力测试文件（多线程同时 invalidate + 加载、TTL 边界、递归加载等场景），依赖验收测试覆盖。
- `invalidate` 中的锁顺序 `state_->mtx → inflight->inflight_mtx` 与 owner 完成路径一致，但如果未来有代码以相反顺序获取这两把锁，则可能死锁。当前实现中无此风险。
