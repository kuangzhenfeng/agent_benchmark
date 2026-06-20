# 作答说明

## 修改摘要

1. **字符串存储修复**：将 `Event` 和 `QueuedEvent` 中的 `std::string_view` 改为 `std::string`，确保数据拥有独立存储。订阅时复制 topic，发布时复制 topic 和 payload。

2. **回调在锁外执行**：drain 方法先在锁内复制事件快照和订阅快照，然后释放锁，在锁外执行回调。

3. **事件批次快照**：drain 时先截取 pending_ 队列的快照并清空，然后处理该快照中的事件。新发布的事件不会在本次 drain 中被处理。

4. **订阅状态检查**：回调执行前检查订阅是否仍然存在，不存在的订阅跳过处理。

5. **异常处理**：回调抛异常时捕获并增加 callback_errors，但仍然增加 delivered 计数并在锁内更新 last_active_tick。

6. **expire_idle 模运算**：使用 `is_expired` 辅助函数，正确处理 32 位时钟回绕和 TTL=0 的情况。

## 验证

运行 `./run_public_checks.sh`：
- `public_checks.cpp` 通过
- `callable_lifetime_checks.cpp` 通过（callable 析构重入场景）

验证命令：
```bash
cd q01-subscription-hub && ./run_public_checks.sh
```

## 剩余风险

1. **多线程竞争**：当前实现在回调执行过程中检查订阅状态，存在微小时间窗口，但符合"回调中取消订阅阻止收到后续事件"的语义。

2. **重入死锁**：已通过在锁外执行回调来避免，但用户回调中调用 drain 可能导致递归处理当前批次，当前实现会跳过新事件，符合"回调中新发布的事件必须留给下一次 drain"的要求。
