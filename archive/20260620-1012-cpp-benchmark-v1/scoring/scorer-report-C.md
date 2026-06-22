# 盲评评分报告（P06，追加轮）— scorer C

> 由独立 scorer subagent 在追加盲评轮输出，仅评分 P06（codex + gpt-5.4）。P01–P05 已归档裁决，未重新评分。评分员只看到匿名 ID。
>
> 评分环境：g++ 9.4.0，`-std=c++17 -Wall -Wextra -Wpedantic -pthread`；ASan runtime 在本环境进程退出阶段会刷 DEADLYSIGNAL（public_checks 无错也受污染），故 ASan 仅用于编译期与“不崩/不死锁”判定，内存错误结论降级为“锁覆盖分析 + 退出码”；TSan 不可用（缺 `libtsan_preinit.o`）。

## P06 总分

| Q1/100 | Q2/100 | Q3/100 | 总分/300 |
|--------|--------|--------|----------|
| 92 | 93 | 93 | **278** |

## Q1 SubscriptionHub

### P06
- **正确性 45 → 42/45**：topic/payload 由 hub 自有 `std::string` 存储（`include/subscription_hub.hpp:43-54`），回调 `Event` 视图指向 drain 局部批次稳定字符串（`src:82`）。drain 锁内一次性截取队列并冻结订阅 ID 上界 `batch_subscription_cutoff`（`src:62-65`），每个事件再单独快照“事件开始时仍订阅、且 `id<cutoff` 的订阅者”（`src:71-80`），故回调新发布事件不进本批次、新订阅不补入、取消订阅阻止后续事件（探针 Q1-B：`unsubscribe(self)` 后 `got=[first]`，delivered=3，PASS）。异常逐个隔离且 `delivered` 含异常回调（`src:84-89`，自带 reentrancy_batch_checks 验证 delivered=5/errors=2）。TTL 无符号模减法（`src:8-14`）含 `ttl==0` 立即过期与 `UINT32_MAX-3→2` 回绕。扣 3：drain 对**每个事件**重新加锁全量遍历订阅快照 + callback 后再锁内线性查找更新 `last_active_tick`（`src:91-97`），O(events×subs)，效率偏低。
- **C++质量 20 → 18/20**：`shared_ptr<CallbackSlot>` 包裹 callable、`make_shared` 锁外构造（`src:21-22`）、`unsubscribe/expire_idle` 锁内仅 `move` shared_ptr、锁外析构（`src:32-47`、`104-124`）。扣 2：drain 内 tick 更新用线性查找而非持引用/下标。
- **约束遵循 15 → 15/15**：未改公开 API；纯 C++17。
- **可维护性 10 → 9/10**：结构清晰、注释到位；drain 偏长，tick 更新与投递耦合。
- **验证证据 10 → 8/10**：run_public_checks.sh exit=0；自带 reentrancy_batch_checks 通过；自写 5 个探针（callable 重入 5 模式 / 取消 / 析构重入 subscribe / 并发 publish-drain）全部 exit=0 无死锁无 crash。ANSWER 承认未上 TSan。扣 2：未自行提供 ASan/多线程强度可复现脚本。
- **小计：92/100**
- **硬性扣分触发：否**。callable 生命周期锁边界未违反（探针 Q1-A 5 模式全 exit=0，`std::function` 复制/移动/销毁未发生在 mutex 内）。
- **关键缺陷**：drain 每事件重新全量快照订阅者 + 锁内线性更新 tick，效率偏低；无 TSan 验证。

## Q2 CoalescingCache

### P06
- **正确性 45 → 42/45**：N 并发同 key miss 合并为单 loader（6 线程 loads==1）；loader/Clock 严格在 cache mutex 外（`src:156/192/200`，clock_reentrancy_checks 通过）；同线程同 key 重入立即返回 `recursive_load`（thread_local `(state*,key)` 栈，`src:24-43`）；loader 抛异常 → capture_failure 降级、全 waiter 失败、不缓存、可重试（探针 fails=5/5、size=0、retry.ok）；in-flight `invalidate` 设 `flight->invalidated`、owner 完成时不回写 ready（探针 owner/waiter 都 ok 且 size==0）；TTL=0 与 capacity=0 不写 ready 但 flight 仍合并；锁序一致无死锁环。扣 3：`flight->invalidated` 为非原子 bool，安全性隐式依赖 `state_->mutex` 保护（owner `src:208` 读 / invalidate `src:247` 写均持 state 锁，当前正确），但缺注释说明该不变量，属可判定性不足。
- **C++质量 20 → 19/20**：`shared_ptr<State>` + `shared_ptr<Flight>` + 独立 `Flight::mutex+cv` 单点发布；LRU `list+unordered_map`；`enforce_capacity` 仅淘汰 ready；`ActiveLoadScope` RAII 规范。
- **约束遵循 15 → 15/15**：公开 API 未变；无忙等/全局锁包 loader。
- **可维护性 10 → 9/10**：get_or_load 较长但 owner/waiter 分支清晰。
- **验证证据 10 → 8/10**：run_public_checks.sh exit=0；自带 recursive_invalidate_checks 三段全通过；探针 Q2-A（完成时刻 Clock 异常 → owner/waiter 均 loader_failed，无逃逸、无永久阻塞）、Q2-B（跨实例同 key 不误判 recursive）、Q2-C（同实例不同 key 正常）、并发不同 key（loads==4）全 PASS。
- **小计：93/100**
- **硬性扣分触发：否**。
- **关键缺陷**：`flight->invalidated` 非原子 bool，安全性隐式依赖 state 锁（当前正确，未注释）；无 TSan。

## Q3 RoutingConfig

### P06
- **正确性 45 → 43/45**：`shared_ptr<const Config>` 快照 + `atomic_load/store_explicit` 发布（`src:36-38/60-70`）；reload 先 `valid()` 完整校验、失败 false 不发布/不递增版本/不通知；版本严格 `+1`（`src:62`）；observer 锁内复制 `shared_ptr<ObserverSlot>` 快照、锁外通知（`src:65-79`），异常 `fetch_add` 错误计数；`current()` per-instance thread_local pin（`src:11-34`）保证旧引用有效且不被另一实例覆盖（探针 Q3-B：a.current()→b.current()→a reload×2，a_old 仍 version=0，PASS）；find_target 最长前缀。扣 2：thread_local pin 按“线程触达实例数”常驻，长生命周期线程 + 多实例场景内存语义未显式约束。
- **C++质量 20 → 19/20**：atomic shared_ptr 安全发布规范；`make_shared<ObserverSlot>` 锁外构造、`unsubscribe` 锁内摘链锁外析构（`src:92-103`）；observer_errors_ atomic；`valid()` 用 `std::set` 查重。
- **约束遵循 15 → 15/15**：未删改公开方法签名。
- **可维护性 10 → 9/10**：职责分离良好；thread_local pin 清理时机缺代码注释。
- **验证证据 10 → 9/10**：run_public_checks.sh exit=0；自带 reload_observer_lifecycle_checks 通过；探针 Q3-A（subscribe 锁外构造，无死锁）、Q3-B（跨实例 pin）、Q3-C（unsubscribe 析构链重入 4 模式无死锁）、Q3-D（4 reader+2 writer 并发，errors=0）全 PASS。
- **小计：93/100**
- **硬性扣分触发：否**。
- **关键缺陷**：thread_local pin 常驻内存语义未显式约束；无 TSan。

## 验证证据总览（探针结果摘要）

- run_public_checks.sh 三题退出码：Q1=0、Q2=0、Q3=0，全部通过。
- Q1 callable 拷贝重入探针（Q1-A）：5 模式（unsubscribe/publish/drain/expire/subscribe）全 exit=0，**硬性封顶条件未触发**。
- Q1 取消语义探针（Q1-B）：`unsubscribe(self)` 后 `got=[first]`，delivered=3，PASS。
- Q1 析构重入探针（Q1-C）：hub 析构期 callable 析构链重入 subscribe，exit=0 无 crash/UAF。
- Q2 完成时刻 Clock 异常探针（Q2-A）：owner/waiter 均 loader_failed，无逃逸、无永久阻塞，PASS。
- Q2 跨实例递归探针（Q2-B）：a.loader 内对 b 同 key 调用 → 正常 ok，无误判 recursive_load，PASS。
- Q3 subscribe 锁外构造探针（Q3-A）：observer 拷贝重入 snapshot，无死锁，PASS。
- Q3 跨实例 current() pin 探针（Q3-B）：a_old 不被 b 覆盖，PASS。
- Q3 unsubscribe 析构重入探针（Q3-C）：4 模式全 exit=0。
- Q3 并发 reload/read 探针（Q3-D）：errors=0，reader 只见合法 target，PASS。
- Q2 loader 抛异常探针：fails=5/5、size=0、retry.ok。

## 评分不确定 / 未验证项

- TSan 完全不可用，所有“无数据竞争”结论基于锁覆盖分析 + ASan 编译 + 退出码/超时，为未验证风险。
- ASan runtime 在本环境异常（退出阶段刷 DEADLYSIGNAL），内存错误定位降级为“锁覆盖分析 + exit=0/crash 检测”。
- Q2 `flight->invalidated` 非 atomic：当前由 state 锁统一保护（正确），缺注释，记为可判定性轻微扣分。
- Q3 thread_local pin 内存累积：ANSWER 已坦承为“有界常驻成本”，属设计权衡。
- 三题 ANSWER.md 引用的命令、测试文件均真实存在并复现通过，未发现编造。

## 尺度校准说明

P06 三题设计与 P01/P02/P03 同档（Q1 `shared_ptr` 包裹 callable + 锁外构造/析构，Q2 flight mutex + LRU，Q3 atomic shared_ptr + per-instance thread_local pin），均通过全部公开检查与全部组合语义探针，未触发任何硬性封顶条件；分数与该档锚点一致，未复制任何其他 P 的分数。
