# 作答说明

## 修改摘要

完全替换了 `CoalescingCache` 的私有实现（`State`），保持公共 API、枚举与值类型不变。核心设计：

**在途状态的所有者（语义 1、2、4、5）**
- 每个 key 的在途加载用一个 `std::shared_ptr<InFlight>` 表示，注册在 `state_->in_flight` 表里。第一个 miss 的线程成为 loader，其它并发调用者成为等待者，各自拷贝一份 `shared_ptr<InFlight>`。
- `InFlight` 自带独立的 `mutex` + `condition_variable`。loader 在 `state_->mutex`（缓存互斥锁）**之外**执行 loader，等待者用各自的 `InFlight::m` 等待 `completed`，与缓存锁完全解耦——因此 loader 重入请求不同 key 会创建/加入各自独立的 slot，互不阻塞。
- 用 `std::thread::id` 区分 loader 与等待者：发现 `in_flight` 中已有同一线程的 slot，即递归请求同一 key，立即返回 `LoadStatus::recursive_load`，绝不等待自己（语义 2）。

**loader 执行与结果发布**
- loader 在锁外执行；返回值或抛异常都被捕获为 `LoadResult`（异常转为 `failed`，不跨 `get_or_load` 边界，语义 4）。
- loader 完成后，loader 线程拿 `state_->mutex` 写回 ready（若可缓存：成功 + `ttl>0` + `capacity>0`），TTL 起点取 loader 完成时的时钟 `expire_time = now() + ttl`（语义 3）。随后从 `in_flight` 移除该 key。
- 最后在 `InFlight::m` 下写入 `result`、置 `completed` 并 `notify_all`：所有等待者共享这一份成功或失败结果。

**等待者唤醒顺序（题目要求说明）**
- 等待者之间不做顺序保证，`cv.notify_all()` 同时唤醒全部；每个等待者拿到的是 loader 的同一份结果（成功或失败）。这满足「所有等待者拿到同一个成功值或失败结果」，无需排队。

**invalidate 竞态（语义 5）**
- `invalidate(key)` 在缓存锁下立即移除 ready 值；若该 key 正在加载，不取消 loader，而是置 `InFlight::discard_result = true`。
- loader 完成后写回 ready 时检查 `discard_result`：为真则不写回（即便成功也不进 ready），但 `in_flight` 仍被移除、等待者仍能收到本次结果。
- 之后到达的调用：若 loader 还在途，会加入这次在途加载（命中 `in_flight` 表，成为等待者）；loader 已结束后，会因 ready 未写回而发起新加载。两种情况都不会写回被 invalidate 的旧值。

**LRU（语义 6、7）**
- ready cache 用 `std::list<std::string>`（front=MRU, back=LRU）+ `unordered_map<key, ReadyNode>`，每个节点存指向链表位置的迭代器。
- 命中且未过期时 `splice` 到链表头部刷新 LRU；插入新项前按需从尾部淘汰。淘汰只针对 ready，不触碰 in_flight；不会因「所有条目都在途」而误删或越界（evict 只作用于 ready 表与链表）。
- `capacity == 0`：`cacheable` 为假，永不写回 ready，`size()` 恒为 0。
- `size()` 只统计 ready，短锁返回；返回的 `LoadResult` 为值拷贝，不暴露内部状态。

## 验证

```text
$ ./run_public_checks.sh          # 公开检查（并发 6 线程合并单次加载）
PUBLIC CHECKS PASSED

$ g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread \
    -I include src/coalescing_cache.cpp tests/boundary_checks.cpp -o /tmp/q2b && /tmp/q2b
ALL BOUNDARY TESTS PASSED          # 多次重复运行同样通过
```

新增 `tests/boundary_checks.cpp`，覆盖：
- 并发 miss 合并：至多一个 loader，所有等待者拿到同一成功值。
- loader 失败：所有等待者拿到失败，失败不进 ready，之后可重新加载。
- loader 抛异常：不跨边界，等待者得到失败，不进 ready。
- 重入不同 key 不死锁、重入同一 key 返回 `recursive_load`。
- TTL：到期重载；未到期命中；`TTL=0` 不缓存；`TTL=0` 时并发仍合并同一次在途加载。
- invalidate：立即移除 ready；加载中 invalidate 不取消 loader 但结果不写回 ready；之后可继续加入/重新加载。
- LRU：访问刷新位置、淘汰正确目标、容量 0 永不保留。

（IDE 内 clangd 因无 include path 报红为误报，g++ 编译运行均通过。）

## 剩余风险

- loader 执行期间，若它在锁外重入请求**同一** key，返回 `recursive_load`；这是题目明确要求的行为，但意味着该内层调用拿不到值（调用方需自行处理该状态）。
- `std::thread::id` 用作 loader 身份标识：同一物理线程内的递归才会被判为 `recursive_load`；不同线程对同一 key 的并发始终走「一个 loader + 多等待者」路径。
- 时钟为注入的 `Clock`（`std::function`），TTL/过期判断基于其返回值；若外部时钟非单调，过期行为依赖外部约定，不在实现控制内。
