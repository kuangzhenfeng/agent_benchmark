# 作答说明 — Q1 SubscriptionHub

## 修改摘要

### 定位到的缺陷（starter）

1. **悬空 `string_view`**：`Subscription::topic`、`QueuedEvent::topic/payload` 全部存 `std::string_view`，而 `subscribe`/`publish` 的输入可能来自临时 `std::string`，导致 `drain` 时回调拿到失效的 topic/payload。
2. **`drain` 持锁回调 + 迭代中改容器**：`drain` 全程持有 `mutex_` 并在循环内调用用户 callback。回调若重入 `subscribe`/`unsubscribe`/`publish`/`drain` 会立即死锁；`unsubscribe` 还在迭代中 `erase`，破坏遍历。
3. **无批次快照**：回调中新 `publish` 的事件被追加进当前正在遍历的 `pending_`，违反「一次 drain 只处理调用开始时已在队列中的事件」。
4. **异常未隔离**：回调抛异常会直接穿透 `drain`，后续订阅者收不到该事件，且 `delivered`/`callback_errors` 统计错误。
5. **32 位时钟过期错误**：`expire_idle` 用 `now_tick > subscription.last_active_tick && now_tick - last_active_tick >= ttl`，回绕时第一个比较为假会漏过期；`ttl=0` 时永远不过期。
6. **callable 生命周期在锁内**（硬性封顶条件）：任何销毁 `Callback`（`~Subscription`、容器搬移/`erase`）都发生在持锁期间，析构若重入 hub 即死锁。

### 修复策略

- **自有存储**：`Subscription::topic`、`QueuedEvent::topic/payload` 改为 `std::string`，输入 `string_view` 在 `subscribe`/`publish` 入口即拷贝。`Event` 视图指向 `QueuedEvent` 的 `std::string`，在该次 drain 批次存活期间有效。
- **shared_ptr 托管订阅 + 锁外回调**：`subscriptions_` 存 `std::shared_ptr<Subscription>`。
  - `drain` 在锁内做两件事：① `swap(pending_)` 取走调用开始时的整批事件；② 把「drain 开始时未取消」的订阅 `shared_ptr` 复制进 `audience` 快照。随后**释放锁**，整个投递阶段无锁，回调可任意重入。
  - `unsubscribe` 在锁内把目标 `shared_ptr` 移出并 `erase`，锁外 `reset()` 触发最后一个所有者的析构 → callable 的析构在锁外完成（满足第 7 条硬性封顶）。同理 `expire_idle` 把淘汰项收集到局部 vector，锁外销毁。
  - `subscribe` 在持锁**前**就完成 `Callback` 的移动/拷贝（用户代码）与对象构造，持锁期间只 `push_back(shared_ptr)`（仅移动控制块指针，无用户代码）。
- **取消语义（per-event 原子）**：`Subscription` 带 `std::atomic<bool> cancelled`。投递每个事件前检查：`cancelled` 的订阅者跳过该事件及其后所有事件。`audience` 在 drain 开始时一次性确定，保证「drain 开始时仍订阅」者至多收一次；回调中新建的订阅不在 `audience` 内，不接收本批任何事件。
- **异常隔离**：每个回调单独 `try/catch(...)`；`delivered` 统计**所有尝试调用次数**（含抛异常者），`callback_errors` 单独计数。单个回调抛异常不影响其余订阅者，也不会在下一次 drain 重投（事件在本次 drain 内已视为处理完）。
- **32 位模运算过期**：`is_expired(now, last, ttl)`：`ttl==0` 直接过期；否则 `elapsed = now - last`（uint32 模减），`elapsed >= ttl` 即过期。在 `ttl < 2^31` 前提下，该比较对回绕正确。活跃 tick 在首次订阅写入，并在**每次实际回调尝试后**（含抛异常）用 `last_active_tick.store(now_tick)` 更新。

## 批次快照 / 取消 / 异常 的处理顺序（题目要求说明）

1. **批次快照**：`drain` 进入 → 锁内 `swap(pending_)` 取走整批（开始时刻已在队列的事件）→ 锁内构建 `audience`（开始时刻未取消的订阅快照）→ **释放锁**。此后回调里 `publish` 的事件进入新的 `pending_`，留给下一次 drain；回调里 `subscribe` 的新订阅不在 `audience`，本批收不到。
2. **逐事件投递**：对每个事件，遍历 `audience`；每个目标投递前查 `cancelled` 原子（取消发生在该事件投递开始之前者跳过）。投递 → 更新 `last_active_tick` → `++delivered`；若抛异常则 `++callback_errors`，但**仍**计入 `delivered` 且**不**重投、**不**中断后续目标/事件。
3. **取消生效点**：回调中 `unsubscribe` 置 `cancelled=true` 并移出 `subscriptions_`。对**当前正在投递的事件**，因目标已用 `shared_ptr` 保活，该事件仍会被投递一次（符合「开始投递时仍订阅者至多一次」）；对该事件**之后**的事件，`cancelled` 检查使其不再接收。

## 验证

```bash
cd q01-subscription-hub
./run_public_checks.sh                              # public_checks + callable_lifetime_checks  → EXIT 0
g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread \
    -I include src/subscription_hub.cpp tests/extra_boundary_checks.cpp -o /tmp/q1_extra && /tmp/q1_extra
# → ALL PASS（8 个边界用例）

# ASan（含 leak detect）
g++ ... -fsanitize=address -fno-omit-frame-pointer ... extra_boundary_checks.cpp && /tmp/q1_asan
# → ALL PASS，无内存错误/泄漏

# TSan 在本机 toolchain 不可用（缺 libtsan_preinit.o），未运行。
```

新增测试 `tests/extra_boundary_checks.cpp` 覆盖：
- 临时字符串视图生命周期（topic/payload 跨调用保活）；
- 批次边界（回调内 publish 不进当前批次，留下次 drain）；
- 回调内新建订阅不收本批事件；
- 回调内 unsubscribe：当前事件仍投递一次，后续事件不再收；
- 异常隔离（一个抛异常不影响另一个；`delivered` 计入尝试次数，`callback_errors` 单列）；
- uint32 回绕过期 + TTL 边界（`elapsed>=ttl`）+ `ttl=0` 立即过期；
- callable 析构重入（drain 投递后，unsubscribe 触发锁外析构并重入 hub）；
- 多线程（4 生产者 + 2 消费者并发 publish/drain）不死锁不崩溃。

## 剩余风险

- **TSan 未跑**：本机 `g++ 9.4` 的 TSan 链接缺失 `libtsan_preinit.o`，无法做正式数据竞争验证。`Subscription::cancelled`/`last_active_tick` 用 `std::atomic`，`shared_ptr` 控制块原子引用计数，理论上无竞争；ASan 通过。
- **`drain` 不是原子「单次调用」并发互斥**：题目未要求 drain 之间互斥，允许多线程并发 drain；`pending_` 与 `audience` 都在锁内快照，逻辑上安全，但「两个 drain 同时跑」会让事件分发到两套 audience——这与单线程语义在极端并发下行为不同（题目允许，未要求 drain 串行）。
- **`expire_idle` 析构重入**：淘汰项在函数末尾锁外销毁，若 callable 析构重入 hub，因为局部 `removed` 已离开锁作用域，重入安全；但未单独为此写测试。
