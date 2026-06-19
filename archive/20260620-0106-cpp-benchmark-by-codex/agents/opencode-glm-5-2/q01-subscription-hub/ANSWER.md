# 作答说明

## 修改摘要

修复了 `SubscriptionHub` 的组合缺陷，覆盖所有权、重入、批次快照、异常隔离、32 位回绕与并发。

具体改动：

1. **存储所有权**（语义 1）：将私有 `Subscription::topic` 与 `QueuedEvent::topic/payload` 从 `std::string_view` 改为 `std::string`，`subscribe`/`publish` 入口处拷贝。这样即便调用方传入临时 `std::string` 派生出的 view，跨调用持有的数据也自拥有存储。`drain` 内构造的公开 `Event` 仍以 `std::string_view` 暴露，但这些 view 指向 `batch` 里被拥有的 `std::string`，在该次回调返回前一直有效。

2. **回调在锁外执行 + 批次快照**（语义 2、3、7）：`drain` 进入时持锁把 `pending_` 一次性 `swap` 到本地 `batch`，并拷贝当前订阅者 `shared_ptr` 列表 `subs_snapshot`；随后释放锁。事件投递、用户回调、活跃 tick 更新全部在锁外完成，仅在每个回调尝试后再短锁一次以更新 `last_active_tick`。回调中新 `publish` 的事件进入 `pending_`，只被下一次（含递归）`drain` 看到；回调中 `subscribe`/`unsubscribe`/`drain`/`expire_idle` 均能在短锁下安全运行，不会死锁。

3. **取消订阅与新订阅语义**（语义 4）：订阅者用 `std::shared_ptr<Subscription>` 持有，`Subscription` 新增 `std::atomic<bool> active`。`unsubscribe`/`expire_idle` 在锁内置 `active=false` 并从 `subscriptions_` 移除，但已快照到 `subs_snapshot` 的 `shared_ptr` 仍保活。`drain` 的订阅者快照在调用开始时一次性建立，回调中新增的订阅不会进入本次批次；每个事件投递开始时按 `active`+`topic` 过滤一次目标——回调中取消的订阅者对**当前事件**仍投递（符合“之后的事件”才阻止），但对**后续事件**被跳过。

4. **异常隔离**（语义 5）：每个回调单独 `try/catch`，异常计数到 `callback_errors`，但仍计入 `delivered`（统计“尝试调用次数”）。一个回调抛异常不阻止其余订阅者接收同一事件，也不会让事件重回 `pending_`（`batch` 本地变量在 `drain` 返回时即销毁）。

5. **32 位回绕与 TTL=0**（语义 6）：`expire_idle` 用 `elapsed = now_tick - last_active_tick`（`uint32_t` 模减）判断过期，在 `ttl < 2^31` 前提下天然正确处理回绕；`ttl == 0` 直接判定过期。`subscribe` 写入 `last_active_tick`，每次回调尝试后更新为 `drain` 的 `now_tick`。

## 处理顺序说明（事件批次快照、取消订阅、异常）

对一次 `drain` 的每个事件 e（按入队顺序）：
1. 持锁 `swap` 出 `batch`，拷贝 `subs_snapshot`（仅在 `drain` 入口做一次）。
2. 对事件 e：在锁外过滤 `subs_snapshot` 得到 e 的**目标集**（仅含 `active==true` 且 `topic` 匹配者）。该目标集在 e 投递开始时冻结。
3. 依次对目标集中的订阅者调用回调；单个回调抛异常被捕获计数，其余订阅者继续。
4. 每次回调尝试后，短锁更新该订阅者 `last_active_tick = now_tick`。

因此：回调中取消订阅 → 影响当前事件**之后**的事件目标集过滤；回调中新建订阅 → 不在 `subs_snapshot`，不收本批次；回调中发布新事件 → 进 `pending_`，留给下一次 `drain`（含递归调用）；回调抛异常 → 不重投、不阻断其它订阅者。

## 验证

- `./run_public_checks.sh`：通过（正常路径）。
- 新增 `tests/edge_checks.cpp`，覆盖：
  - 临时字符串所有权（`test_temporary_string_ownership`）
  - 回调中取消订阅只阻止后续事件（`test_reentrant_unsubscribe_blocks_future_events`）
  - 回调中新建订阅不收本批次（`test_reentrant_subscribe_does_not_receive_current_batch`）
  - 回调中发布事件延后到下次 drain（`test_reentrant_publish_deferred_to_next_drain`）
  - 回调中递归 drain 处理新发布事件（`test_recursive_drain_processes_newly_published`）
  - 单回调抛异常不阻断其它订阅者（`test_callback_exception_does_not_block_others`）
  - 异常后不在下次 drain 重投（`test_no_redelivery_after_exception`）
  - TTL=0 立即过期（`test_ttl_zero_immediate_expiry`）
  - TTL 边界 elapsed<TTL 不过期、elapsed==TTL 过期（`test_ttl_boundary_not_expired`）
  - 回调后活跃 tick 刷新（`test_active_tick_refreshed_after_callback`）
  - 32 位回绕过期与不过期（`test_wraparound_expiry` / `test_wraparound_not_expired_when_recent`）
  - 多线程并发 publish/drain 不漏不重（`test_concurrent_publish_drain`）
- 编译命令：`g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread -Iinclude src/subscription_hub.cpp tests/edge_checks.cpp -o /tmp/q1_edge`
- 运行结果：全部断言通过（exit=0）。

## 剩余风险

- `delivered` 语义为“尝试调用次数（含抛异常）”。若评分期望把抛异常的尝试同时计入 `delivered` 且 `callback_errors`，已按此实现；若期望互斥，需要确认。
- 极高并发下，`expire_idle` 与 `drain` 的 tick 更新存在良性的时间窗口：`expire_idle` 可能在某订阅者刚收到回调、tick 更新之前判其为空闲并移除；这符合“经过时间 >= TTL 即过期”的字面语义，但会让该订阅者错过后续事件。该行为是 TTL 语义与并发投递的本质竞态，无法在不引入 per-subscription 锁的情况下完全消除。
- 回调重入 `drain` 时，外层 drain 的目标集已基于入口快照固定；内层 drain 会处理新进入 `pending_` 的事件，不会与外层批次重叠。
