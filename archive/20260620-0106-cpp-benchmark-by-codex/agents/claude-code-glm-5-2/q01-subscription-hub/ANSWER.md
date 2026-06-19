# 作答说明

## 修改摘要

定位到的缺陷与修复策略：

1. **临时字符串悬空（语义 1）**：原 `Subscription` / `QueuedEvent` 用 `std::string_view` 持有 topic/payload，跨调用保存的视图会随临时 `std::string` 析构而悬空。改为自有存储：`SubscriptionData::topic` 与 `QueuedEvent::topic/payload` 改为 `std::string`。`Event` 视图指向本轮 drain 局部 `QueuedEvent`，在该次回调返回前有效。

2. **回调重入死锁与失效引用（语义 2、3）**：原 `drain` 全程持有 `mutex_`，回调重入 `drain/publish/subscribe/unsubscribe` 会死锁。改为：drain 开始时在**锁内**对「调用开始时已入队的事件」与「开始时仍订阅的订阅者」各取一份快照（shared_ptr 副本）并清空 `pending_`，随后**完全在锁外**投递。回调中 `publish` 的新事件留在 `pending_`，留给下一次 drain；回调中 `drain` 处理的是新一轮，不会无限递归当前批次。

3. **订阅者所有权（语义 2、4）**：订阅改为 `std::shared_ptr<SubscriptionData>`，drain 锁外投递期间即使 `unsubscribe`/`expire_idle` 把对象移出容器，回调指针仍存活（不会用失效引用）。`unsubscribe` 既从列表移除又置 `alive=false`；drain 投递每个事件前检查 `alive`，从而「取消只阻止当前事件之后的事件」、且「回调中新建的订阅不在本次 drain 快照中，不接收已截取事件」。

4. **重复投递与异常（语义 5）**：原实现回调抛异常会中断整轮并跳过 `pending_.clear()`，导致重复投递且其余回调被跳过。改为对每个订阅者单独 `try/catch`：单个回调异常不阻止其余回调；事件已离开队列，对已尝试订阅者视为处理完成，下次 drain 不重复；`delivered` 计数包含抛异常的尝试，`callback_errors` 单独累加。

5. **32 位时钟回绕（语义 6）**：原 `now_tick > last_active_tick` 条件在回绕时会错判（now 反而“小于” last）。改为模运算 `elapsed = now - last`，在 `ttl < 2^31` 下 `elapsed >= ttl` 即过期；`ttl == 0` 立即过期。活跃 tick 在「首次订阅」与「每次实际尝试回调后」更新（`last_active_tick` 为 atomic，锁外投递时写）。

6. **多线程（语义 7）**：内部锁不在用户回调期间持有；drain 投递段无锁，可并发执行其它公共方法。

处理顺序（题目要求说明）：**事件批次快照 → 订阅者快照（同时清空 pending）→ 锁外逐事件投递**；投递时先判 `alive`（取消订阅优先），再判 topic 匹配，再 try/catch 调用（异常隔离）。

## 验证

```text
$ ./run_public_checks.sh          # 公开检查
PUBLIC CHECKS PASSED

$ g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread \
    -I include src/subscription_hub.cpp tests/boundary_checks.cpp -o /tmp/q1b && /tmp/q1b
ALL BOUNDARY TESTS PASSED
```

新增 `tests/boundary_checks.cpp`，覆盖：
- 临时 string 输入的所有权（topic/payload 自有存储）。
- 回调中 `publish` 的事件留给下次 drain；回调中重入 `drain` 不死锁、不无限递归当前批次。
- 回调中取消自身订阅：当前事件仍投递，后续事件跳过；回调中新建订阅不接收本次 drain 已截取的事件。
- 单个回调抛异常：其余回调仍执行，`delivered` 含异常次数，`callback_errors==1`，事件不被下次 drain 重复投递。
- 32 位回绕：`last=0xFFFFFFFA, now=5` 经模运算 elapsed=11，`ttl=100` 未过期；`ttl=0` 立即过期。
- 多线程：8 线程并发 `publish` + 并发 `drain`，最终投递计数 == 总发布数且队列清空。

（IDE 内 clangd 因无 include path 报红为误报，g++ 编译运行均通过。）

## 剩余风险

- drain 的「每个订阅者每个事件至多收到一次」依赖「事件已离开队列」实现去重，而非显式 ack；语义 5 要求「该事件对已尝试调用的订阅者仍视为处理完成，不能在下一次 drain 被重复投递」已满足（事件整体从队列移除），但若未来要求按订阅者级别部分重投，需要额外记账。
- 多次 drain 并发时，每次 drain 只处理自己开始时刻的快照，两个并发 drain 之间的事件分配由各自取快照的时序决定（语义未规定跨 drain 的顺序），属可接受的不确定性。
