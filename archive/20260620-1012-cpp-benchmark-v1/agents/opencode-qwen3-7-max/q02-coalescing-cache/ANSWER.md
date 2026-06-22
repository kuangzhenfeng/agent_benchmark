# Q2: 实现并发请求合并缓存

## 修改摘要

### 状态机设计

**State 结构**
- `flight_map`: `unordered_map<key, shared_ptr<FlightFuture>>` 跟踪在途加载
- `ready_cache` + `ready_order`: ready 缓存值与 LRU 淘汰顺序
- 单一 `std::mutex mtx` 保护所有内部状态

**FlightFuture**
- 每个在途 flight 持有一个独立的 mutex + condition_variable
- `result_set`、`result`、`invalidated` 标志
- 通过 `shared_ptr<FlightFuture>` 确保 flight 生命周期延续到所有等待者释放后

### 锁边界设计

1. **Loader 在锁外执行**：flight 创建后立即释放锁，然后调用 loader，完成后重新加锁更新状态
2. **Clock 在锁外执行**：TTL 检查时先在锁内读取 ready 值，释放锁后调用 clock，再决定是否提供缓存值
3. **等待者唤醒**：owner 在 flight's mutex 下设置 result_set 并 notify_all，等待者通过 `cv.wait` 阻塞直到结果就绪

### 在途状态的所有者与等待者

- **Owner（第一个调用者）**：
  1. 在锁内检查 ready_cache（未命中）和 flight_map（不存在）
  2. 创建 FlightFuture 并插入 flight_map
  3. 释放锁，调用 loader
  4. 调用 clock 获取完成时间
  5. 加锁：检查 invalidated 标志，决定是否写入 ready_cache，执行 LRU 淘汰
  6. 通知等待者
  7. 从 flight_map 移除 flight

- **Waiter（后续调用者）**：
  1. 在锁内找到已有 flight
  2. 在 flight's mutex 下等待 result_set
  3. 获取结果并返回

### Invalidate 竞态处理

- `invalidate(key)` 移除 ready_cache 并标记 flight.invalidated
- Flight 完成后不写回 ready_cache，但等待者仍获得结果
- 之后的新调用可加入在途 flight 或开始新加载

### 递归加载检测

使用 `thread_local unordered_set<string> loading_keys` 跟踪当前线程正在加载的 key，检测到同一 key 的递归调用时立即返回 `recursive_load`。

### LRU 淘汰

- `ready_order` 是 `list<string>` 维护 LRU 顺序（front 为最久未使用）
- 每次访问 ready 值时移到 list 末尾
- 容量超限时从 front 淘汰
- 容量为 0 时永不保留 ready 值

## 验证

```bash
./run_public_checks.sh
# public_checks.cpp: PASS (6 并发线程加载同一 key，loader 只调用 1 次)
# clock_reentrancy_checks.cpp: PASS (clock 内调用 invalidate 不死锁)
```

## 剩余风险

- TTL 检查与 clock 调用之间存在竞态窗口：缓存值可能在 TTL 检查通过后、返回前过期，但返回的值在检查时是有效的
- `ready_order.remove(key)` 是 O(n) 操作，高容量下可能成为瓶颈，但功能正确
- 未添加更多并发压力测试
