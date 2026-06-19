# 作答说明

## 修改摘要

完整实现 `CoalescingCache`。核心数据结构：`std::list` + `std::unordered_map` 组成 ready cache（LRU），`InflightMap`（`unordered_map`）管理在途加载。

### 状态机

每个 key 有三种状态：
1. **ready**：已缓存且未过期（ready_list + ready_map）
2. **in-flight**：正在加载（inflight_map，含 promise/future）
3. **absent**：既不在 ready 也不在 inflight

### 锁边界与请求合并

所有内部状态由单个 `std::mutex` 保护。`get_or_load` 流程：
1. 加锁：检查 ready（命中且未过期 → 刷 LRU 位置后返回）；检查过期条目并淘汰。
2. 检查 inflight：存在则获取 shared_future；若调用者线程 = loader 线程则检测递归（返回 `recursive_load`）。
3. 无命中无在途：创建 promise/shared_future，将自身线程 ID 记录为 loader_thread；释放锁。
4. 在锁外执行 loader（支持 loader 重入请求不同 key）。
5. loader 完成后重新加锁：读取 invalidated 标志、`set_value`、从 inflight 移除，按条件写入 ready cache（ok && 未 invalidated && capacity>0 && ttl>0）。

### 失效竞态处理

`invalidate(key)` 在锁内：删除 ready 条目（若存在）；若在途，置 `invalidated=true`。
- loader 完成后读取 `invalidated`；若为 true，只通知等待者（通过 promise），不写回 ready cache。
- invalidate 发生在 loader 完成后、写 ready 前：锁序列化，要么 invalidate 成功（invalidated=true），要么 loader 已完成写入并被 invalidate 随后删除。
- 并发新调用可继续加入在途加载（inflight_map 仍有该条目），结果不缓存。

### 在途状态的所有者与等待者唤醒

只有成为 loader 的线程运行 loader 函数（在锁外）。等待者持 shared_future 并在 `future.get()` 上阻塞。loader 完成后 `promise->set_value` 一次性唤醒所有等待者。无忙等、无轮询 sleep。

### LRU 淘汰

- ready_list front = MRU；访问时 splice 到 front。
- 淘汰：从 ready_list 尾部（LRU）逐条删除，直到 `ready_map.size() < capacity`。
- 在途加载不占 ready cache 槽位，不会被淘汰。
- capacity=0 永不写入 ready。ttl=0 不缓存（但并发调用仍合并同一次在途加载）。

## 验证

运行环境：g++ C++17，`./run_public_checks.sh` 通过。

新增测试覆盖：
- **缓存命中**：同 key 第二次 get_or_load 返回缓存值，不触发 loader。
- **TTL 过期**：now 超过 expiry 后重新加载。
- **loader 失败合并**：4 线程并发请求同一 key，loader 只执行一次，所有等待者得到 failed。
- **loader 异常合并**：同上，loader throw，异常不跨越 API 边界。
- **invalidate ready 后再加载**：invalidate 后缓存大小减 1，再次 get_or_load 触发新 loader。
- **LRU 淘汰**：capacity=2，加入第三个 key 后淘汰最早未访问的。
- **capacity=0**：加载后不保留 ready，连续两次 get_or_load 都触发 loader。
- **TTL=0 不缓存**：加载后 size=0，下次 get_or_load 重新加载。
- **loader 重入**：外层 loader 请求同一 key 得 recursive_load，请求不同 key 正常返回。
- **invalidate 在途加载**：loader 运行中 invalidate，loader 完成后 ready 不被写回，后续 get_or_load 触发新加载。

## 剩余风险

- `ready_map.size()` 在 evict 循环中每次迭代都重新计算，可优化为缓存大小。
- 若 loader 执行时间极长且大量线程同时等待同一 key，`future.get()` 等待者可能占用线程资源，但符合规范要求（无忙等）。
