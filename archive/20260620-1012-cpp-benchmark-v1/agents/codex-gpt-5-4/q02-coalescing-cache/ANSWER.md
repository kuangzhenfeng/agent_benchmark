# 作答说明

## 修改摘要

- 以 `State` 维护两类状态：`ready_cache + LRU` 负责可复用的成功值，`inflight` 负责同 key 的在途 flight。`get_or_load` 总是先看 `inflight`，确保同一时刻同 key 至多一个 owner loader。
- `Clock` 和 `loader` 都严格放在缓存互斥锁外执行。ready 命中是否过期先在锁外取时钟，再回到锁内复核；owner loader 完成后也在锁外取 TTL 起点时钟，再统一提交结果。
- 同线程同 cache/同 key 的递归请求用线程局部栈立即返回 `LoadStatus::recursive_load`，避免 loader 重入等待自己；不同 key 仍可正常重入。
- flight 的完成使用单独的 `Flight::mutex + condition_variable` 发布结果。owner 在 flight 仍可发现时先决定最终 `LoadResult`、是否写入 ready cache，再把结果写入 flight、唤醒等待者，最后从 `inflight` 移除；waiter 都通过同一个 flight 同步关系观察一致结果。
- `invalidate(key)` 会立即删除 ready cache，并给在途 flight 打上 `invalidated` 标记。这样现有等待者仍返回本次结果，但 owner 完成时不会把该结果写回 ready cache；flight 消失后的后续请求会重新建新 flight。
- ready cache 采用 LRU：ready 命中刷新位置，插入成功值时按容量淘汰最久未使用项；`ttl == 0` 或加载失败/抛异常都不会写入 ready cache，但并发请求仍会复用同一 flight。

## 验证

- `cd q02-coalescing-cache && ./run_public_checks.sh`
- `cd q02-coalescing-cache && g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread -Iinclude src/coalescing_cache.cpp tests/recursive_invalidate_checks.cpp -o /tmp/q02-extra && timeout 5s /tmp/q02-extra`
- 新增 `tests/recursive_invalidate_checks.cpp`，覆盖同 key 递归保护、在途 `invalidate` 禁止回写，以及 `ttl == 0` 时“不缓存但仍合并 flight”。

## 剩余风险

- 目前没有额外加入长时间随机压力或 TSAN；多线程可见性主要通过 flight 单点发布和互斥锁顺序保证。
