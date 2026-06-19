# Q2：实现并发请求合并缓存

实现 `CoalescingCache`。它被 API 网关用于消除同一 key 的缓存击穿：第一个调用者执行 loader，其他并发调用者等待并复用该次结果。现有骨架可编译，但没有正确实现。

## 运行方式

```bash
./run_public_checks.sh
```

公开检查在 starter 状态下预期失败；完成实现后应通过。请增加覆盖并发与失效边界的测试。

## 必须满足的语义

1. 对同一 key 的并发 cache miss，任一时刻至多执行一个 loader。所有等待者拿到该次 loader 的同一个成功值或失败结果。
2. loader 必须在内部互斥锁之外执行。loader 可重入地请求**不同** key，不能导致全局死锁；若 loader 重入请求同一 key，内层调用必须立即返回 `LoadStatus::recursive_load`，不得等待自己。
3. 成功结果以 loader **完成时**的时钟作为 TTL 起点。到期时重新加载；TTL 为 0 时不缓存，但并发调用仍要合并同一次在途加载。
4. loader 返回失败或抛异常时，等待者都得到失败结果，失败不进入 ready cache，之后的调用可以重新加载。不得让异常跨越 `get_or_load` 的 API 边界。
5. `invalidate(key)` 必须立即移除 ready 缓存值。若该 key 正在加载，不取消 loader；该次结果可返回给现有等待者，但不得在 loader 完成后重新写回 ready cache。之后的调用可继续加入这次在途加载。
6. ready cache 使用最近最少使用（LRU）淘汰，容量由构造函数给定。访问 ready 值会刷新其 LRU 位置；不得淘汰在途加载，也不得因为所有条目都在途而违反内存安全。容量为 0 时永不保留 ready 值。
7. 所有公开方法可并发调用。`size()` 只统计 ready cache；返回结果应保持自包含，不暴露内部可变状态。

## 约束与验收

- 保持 `coalescing_cache.hpp` 中的公共 API、枚举和值类型不变；可以替换私有实现。
- 仅限 C++17 标准库；不要使用忙等、全局锁包住 loader、轮询 sleep 作为同步方案。
- 需要处理容量、TTL、loader 异常、失效与重入的组合，而不仅是简单双检锁。
- 在 `ANSWER.md` 中解释在途状态的所有者、等待者唤醒顺序和 invalidate 竞态处理。
- 验收会重复运行多线程压力测试，并在 loader 内控制时钟和调用 `invalidate`。偶现死锁、重复加载或不确定结果均视为失败。
