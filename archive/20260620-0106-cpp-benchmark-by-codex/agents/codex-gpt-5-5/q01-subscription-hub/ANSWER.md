# 作答说明

## 修改摘要

- `Subscription` 的 topic 和队列事件的 topic/payload 改为 `std::string` 所有权，因而临时字符串不会在 `subscribe` 或 `publish` 返回后悬空。
- `drain` 在短锁区间内交换出当时的队列并复制订阅对象的共享所有权快照；回调期间发布的事件留在新队列，新增订阅也不在本次快照内。用户回调从不在内部锁下执行。
- 订阅取消和过期会先将订阅标为非活跃。每次实际调用前再次在锁下检查该标记，因此回调中的取消会阻止同一批次后续事件；快照保留对象生命周期，避免取消后使用失效引用。
- 每次尝试回调都独立捕获异常、计入 `delivered` 和 `callback_errors`，并在回调返回后更新仍活跃订阅的 tick。过期判断使用 `uint32_t` 减法的模运算，TTL 为 0 时自然立即到期。

## 验证

- `./run_public_checks.sh`（在 `q01-subscription-hub` 中执行）：通过。
- `g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread -I q01-subscription-hub/include q01-subscription-hub/src/subscription_hub.cpp q01-subscription-hub/tests/edge_checks.cpp -o /tmp/q01-edge && /tmp/q01-edge`：通过。
- 新增边界检查覆盖临时字符串所有权、回调发布的下一批次语义、回调中取消订阅、异常隔离和 `uint32_t` 回绕过期。

## 剩余风险

- 回调可能被不同调用 `drain` 的线程并发执行；其自身共享状态需要由回调实现方同步。Hub 保证其内部状态同步且不在回调期间持锁。
