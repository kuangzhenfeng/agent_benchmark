# 作答说明

## 修改摘要

- 实现了由单个 `State::mutex` 保护的 ready LRU 和 in-flight 表。miss 的首个
  调用者插入 `Flight` 后在锁外运行 loader；同 key 后续调用者等待该 `Flight`
  的 condition variable，并在同一把 mutex 下读取最终结果。
- `Clock` 在 ready freshness 判断前、以及成功 loader 完成后调用，均在所有缓存锁
  外。thread-local 活跃调用栈阻止 loader（以及 Clock）在同一线程重入同一 key
  时等待自己，并返回 `recursive_load`；不同 key 正常可重入。
- owner 在同一个临界区内完成最终结果赋值、`invalidate` 标志判定、可选 LRU
  写入、`done` 发布和 in-flight 删除，之后唤醒等待者。因此新调用者只能观察到
  ready 值或创建/加入新的完整 flight。
- 成功项以 loader 完成时刻和该 flight 的 TTL 记录；TTL 为零或 capacity 为零时
  仍合并在途请求但不缓存。失败和异常不写 ready。失效同步移除 ready 值，并标记
  在途 flight，使其结果只能返回给现有等待者而不能回填缓存。

## 验证

已执行并通过：

```bash
./q02-coalescing-cache/run_public_checks.sh
```

新增 `tests/edge_semantics_checks.cpp`，覆盖同 key 递归加载、完成时 TTL 边界、
缓存命中和 loader 内 `invalidate` 对回填的抑制。公开检查覆盖多线程合并和 Clock
重入调用 `invalidate`。
另以 `clang++ -std=c++17 -Wall -Wextra -Wpedantic -pthread` 逐个编译并运行三项
检查，均通过。

## 剩余风险

公开检查和新增确定性边界检查均通过；未运行 ThreadSanitizer 或独立的长时间随机
并发压力测试。
