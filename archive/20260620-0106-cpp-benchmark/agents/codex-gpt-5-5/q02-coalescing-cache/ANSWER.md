# 作答说明

## 修改摘要

- 实现了三态存储：ready 值、按 key 的共享 `InFlight` 状态和 LRU 链表。创建 in-flight 状态的调用者是 loader 所有者；其余调用者在同一条件变量上等待完成，并复制同一结果。
- loader 和时钟函数均在缓存锁外执行。线程局部的活动加载栈检测同一缓存、同一 key 的重入并立即返回 `recursive_load`；不同 key 可以正常重入。
- 成功 loader 返回后才读取时钟，作为 ready 值的 TTL 起点。失败和异常被转换为失败结果并唤醒全部等待者，但不写入 ready cache。容量为 0 或 TTL 为 0 时同样合并在途调用，只是不保留 ready 值。
- `invalidate` 会删除 ready 值，并把匹配的 in-flight 标记为不可缓存；它不取消 loader 或等待者，故该次结果仍可返回，但完成时不会重新写回。ready cache 的命中会移动到 LRU 队首，插入后从队尾淘汰。

## 验证

- `./run_public_checks.sh`（在 `q02-coalescing-cache` 中执行）：通过。
- `g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread -I q02-coalescing-cache/include q02-coalescing-cache/src/coalescing_cache.cpp q02-coalescing-cache/tests/edge_checks.cpp -o /tmp/q02-edge && /tmp/q02-edge`：通过。
- 新增边界检查覆盖 loader 完成时计时的 TTL 边界、同 key 递归与不同 key 重入、LRU 命中刷新/淘汰，以及在 loader 等待期间失效后结果不回填 ready cache。

## 剩余风险

- TTL 使用无符号 elapsed-time 比较，适用于单调时钟及常规的未跨完整 `uint64_t` 周期的 TTL；题面未规定时钟完整回绕的语义。
