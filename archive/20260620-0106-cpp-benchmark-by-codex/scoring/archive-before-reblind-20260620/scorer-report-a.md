# scorer-A 盲评报告（C++17 benchmark，匿名 P01..P05）

评分依据：题面 `QUESTION.md` 验收为准 + 源码事实（`文件:行`）+ 实跑验证证据。
环境：g++ 9.4.0，`-std=c++17 -Wall -Wextra -Wpedantic -pthread`。各题 `run_public_checks.sh` 均实跑通过（退出码 0）。除公开检查外，本评分者额外编写并发/重入/边界探针对所有 Pxx 实跑核验（见每节“验证证据”）。

---

## P01 评分

### Q1 subscription-hub
- 正确性 /45：43 — 满足全部 7 条语义。`SubscriptionData`/`QueuedEvent` 用 `std::string` 自有存储（`subscription_hub.hpp:43-59`），drain 在锁内快照事件与订阅者并清空 `pending_`、锁外投递（`subscription_hub.cpp:55-98`），回调中 publish 进 `pending_` 留下次 drain；`alive` 原子标志 + 锁外 shared_ptr 实现取消语义且无失效引用；逐订阅者 try/catch、`delivered` 含异常计数；过期用 `elapsed(now,last)` 模减 + ttl==0 立即过期（`subscription_hub.cpp:100-118`）。实跑：`boundary_checks.cpp` 7 项全过（临时串、重入 publish/drain、回调中取消/新增、异常隔离、32 位回绕、8 线程并发）+ 本评分者探针（回调中 reentrant publish/subscribe/unsubscribe 不死锁；unsub-other-in-callback 正确；异常后无重投）。轻微：`unsubscribe` 标 `alive=false` 与 erase 都在锁内、但销毁最后一个 callback shared_ptr 发生在 erase 处锁内（`subscription_hub.cpp:41-46`），若 callback 析构重入 hub 会重锁 `std::mutex`——但正常路径不会触发，未观察到死锁，记为未验证风险（见下）。
- C++质量 /20：19 — shared_ptr+atomic<bool> 合理，异常安全，无 UB；注释清晰。
- 约束 /15：15 — 公共 API 不变，目录隔离，无 C++17 外依赖。
- 可维护 /10：9 — 清晰，处理顺序文档化充分。
- 验证 /10：10 — 实跑 boundary_checks 全过 + 公开检查通过；ANSWER.md 诚实标注剩余风险。
- 小计：96
- 关键缺陷：callback 销毁（最后一个 shared_ptr）在 `unsubscribe`/`expire_idle` 锁内 erase 时发生，理论上 callback 析构重入 hub 会死锁（参考解答要求延迟销毁）。但 `delivered` 已在外层、回调本体从不持锁；该风险极窄且未在测试中触发。

### Q2 coalescing-cache
- 正确性 /45：45 — per-key `InFlight{独立 mutex+cv}`、loader 在 state 锁外执行（`coalescing_cache.cpp:115-123`），等待者用 `InFlight::m` 等待，不持缓存锁；`std::thread::id` 判同 key 递归返回 `recursive_load`（`coalescing_cache.cpp:94-97`）；TTL 用 loader 完成时钟 `expire_time=now()+ttl`（`coalescing_cache.cpp:133-135`）；失败/异常不缓存、唤醒全部等待者；`invalidate` 设 `discard_result` 不取消 loader（`coalescing_cache.cpp:166-179`）；LRU list+map，命中 splice 刷新、淘汰只动 ready、cap=0 不缓存。实跑：`boundary_checks.cpp` 8 项全过（并发合并单 loader、失败/异常共享、递归、TTL 边界、TTL=0 合并、invalidate 竞态、LRU）+ 本评分者探针（8 线程 5 次合并均 loads=1/ok=8；20 线程跨 key 无死锁；invalidate-during-load size=0；递归 diff=ok/same=recursive_load）。
- C++质量 /20：19 — 两层锁分离干净；`shared_ptr<InFlight>` 保活使 owner 从 in_flight 移除后等待者仍能完成，无 broken-promise。
- 约束 /15：15 — API/枚举不变，纯 C++17，无忙等/全局锁包 loader。
- 可维护 /10：10 — 状态机清晰、ANSWER.md 逐语义说明。
- 验证 /10：10 — 实跑全覆盖。
- 小计：99
- 关键缺陷：无明显缺陷。

### Q3 routing-config
- 正确性 /45：44 — `shared_ptr<const Config> current_` 原子发布（`routing_config.cpp:47-68`），先 `valid()` 后发布、失败不变；`current()` 用 `thread_local shared_ptr` pin（`routing_config.cpp:10-27`），跨 reload 引用有效（实跑：100 次 reload 后旧引用仍 version=0），下次 current 切换；reload 锁内拷贝观察者副本、锁外通知、异常隔离计数；`find_target` 单快照最长前缀匹配（`routing_config.cpp:95-110`）。实跑：`boundary_checks.cpp` 6 项全过（校验失败不变、版本加一、current 保活、最长前缀、observer 重入/异常、4 reader+4 writer 并发 version==800）+ 探针（observer copy/dtor 探针未死锁——但 reload 仍在锁内拷贝 observers_ map 见下；current ref lifetime；observer 重入 find_target/unsubscribe/subscribe a=1 b=1）。轻微：reload 在锁内 `to_notify` 拷贝 `Observer`（std::function 复制）发生持锁（`routing_config.cpp:64-67`），若 Observer 复制/析构重入 API 会重锁死锁——探针显示当前 lambda 不重入故未触发，记为未验证风险。
- C++质量 /20：19 — shared_ptr 发布 + thread_local pin 方案正确；`valid()` 不访问共享状态故锁外安全。
- 约束 /15：15 — 签名不变，纯 C++17，无平台 RCU/全局静态路由表。
- 可维护 /10：9 — 清晰。
- 验证 /10：10 — 实跑全覆盖。
- 小计：97
- 关键缺陷：reload 通知快照在锁内复制 `std::function`（`routing_config.cpp:65-67`）；参考要求把 observer 封装为 shared_ptr、锁内只拷贝指针。Observer 析构/复制重入 API 的极端场景未验证（记为风险，未臆测扣全分）。

## P01 汇总
| 题 | 正确性 | C++质量 | 约束 | 可维护 | 验证 | 总分 |
|----|--------|---------|------|--------|------|------|
| Q1 | 43/45 | 19/20 | 15/15 | 9/10 | 10/10 | 96/100 |
| Q2 | 45/45 | 19/20 | 15/15 | 10/10 | 10/10 | 99/100 |
| Q3 | 44/45 | 19/20 | 15/15 | 9/10 | 10/10 | 97/100 |
| 平均 | 44.0 | 19.0 | 15.0 | 9.3 | 10.0 | **97.3** |

---

## P02 评分

### Q1 subscription-hub
- 正确性 /45：38 — 多数语义正确：自有 string 存储（`subscription_hub.cpp:10-11,26-27`），drain `batch.swap(pending_)` 快照、锁外投递（`subscription_hub.cpp:30-83`），逐回调 try/catch、`delivered` 含异常，过期模减 + ttl==0（`subscription_hub.cpp:85-101`）。实跑探针全过（重入不死锁、unsub-other、异常隔离、回绕、inner drain）。**缺陷**：订阅用值类型 `vector<Subscription>` 而非 shared_ptr，drain 拷贝整份 `subs_snapshot`（含每份 `std::function` 拷贝，`subscription_hub.cpp:37`）开销大且锁内复制用户 callback；存活检查每次回调前对 `subscriptions_` 做 `std::any_of` 线性扫描并加锁（`subscription_hub.cpp:51-59`）——虽不死锁但效率差。`unsubscribe` 锁内 erase 销毁最后一个 callback（`subscription_hub.cpp:15-21`）存在 callback 析构重入死锁的未验证风险（与 P01 同类）。未提供 boundary 测试文件（ANSWER.md 自承“未新增边界压力测试”）。
- C++质量 /20：16 — 实现正确但值拷贝订阅列表每次 drain 触发 N 次 std::function 复制，设计偏重。
- 约束 /15：15 — API 不变，纯 C++17。
- 可维护 /10：7 — 存活检查用 any_of 全表扫描、tick 更新又全表扫描，不够清晰。
- 验证 /10：5 — 仅依赖公开检查；ANSWER.md 明确未新增边界测试，验收依赖外部。本评分者探针虽通过，但参评者自身验证证据薄弱。
- 小计：81
- 关键缺陷：无 shared_ptr 订阅保活 → drain 全量拷贝 callback；存活/tick 检查全表线性扫描+逐次加锁；无自测。

### Q2 coalescing-cache
- 正确性 /45：42 — 两层锁（state `mtx` + per-key `inflight_mtx`），loader 锁外执行（`coalescing_cache.cpp:154-163`），同线程递归返回 `recursive_load`（`coalescing_cache.cpp:127-129`），完成时钟 TTL（`coalescing_cache.cpp:165-166,182`），失败/异常不缓存，invalidate 设 `invalidated` 锁顺序一致（`coalescing_cache.cpp:191-202`），LRU list+map。实跑探针全过（8 线程合并、跨 key 无死锁、invalidate-during-load、递归 diff/same）。**缺陷**：未提供 boundary 测试文件（仅公开检查，ANSWER.md 自承“未新增独立边界压力测试”）；`invalidate` 取 `state_->mtx` 后再取 `inflight_mtx`（`coalescing_cache.cpp:198-200`）而 owner 完成路径只取 inflight_mtx（已脱离 state 锁），锁序一致无死锁，但 owner 完成→notify→写 ready 的顺序（先 `done=true`+notify 再写 ready，`coalescing_cache.cpp:169-186`）使得 notify 后极短窗口内新调用者可能未命中 ready 又创建新 flight（短暂重复加载窗口，非正确性破坏，记为轻微）。
- C++质量 /20：18 — 设计合理；shared_future/promise 保活处理得当。
- 约束 /15：15 — API 不变，无忙等/全局锁。
- 可维护 /10：8 — 清晰。
- 验证 /10：5 — 仅公开检查，无自测；本评分者探针通过但参评者自身证据薄弱。
- 小计：88
- 关键缺陷：无自测文件；owner 先 notify 后写 ready 的窗口。

### Q3 routing-config
- 正确性 /45：42 — `shared_ptr<const Config> current_snapshot_` 发布、先校验后发布、版本加一（`routing_config.cpp:38-51`），`current()` thread_local pin（`routing_config.cpp:10-20`）跨 reload 有效（实跑验证），锁外通知 + 异常隔离，find_target 单快照最长前缀。实跑探针全过（current lifetime、observer 重入 a=1 b=1）。**缺陷**：未提供 boundary 测试（ANSWER.md 自承“未新增独立并发压力测试”）；reload 锁内拷贝 `observers_` map（复制 std::function）持锁（`routing_config.cpp:50`），与 P01 同类的未验证风险。
- C++质量 /20：18 — 正确。
- 约束 /15：15 — 签名不变，纯 C++17。
- 可维护 /10：8 — 清晰简洁。
- 验证 /10：5 — 仅公开检查，无自测。
- 小计：88
- 关键缺陷：无自测文件；observer std::function 锁内复制。

## P02 汇总
| 题 | 正确性 | C++质量 | 约束 | 可维护 | 验证 | 总分 |
|----|--------|---------|------|--------|------|------|
| Q1 | 38/45 | 16/20 | 15/15 | 7/10 | 5/10 | 81/100 |
| Q2 | 42/45 | 18/20 | 15/15 | 8/10 | 5/10 | 88/100 |
| Q3 | 42/45 | 18/20 | 15/15 | 8/10 | 5/10 | 88/100 |
| 平均 | 40.7 | 17.3 | 15.0 | 7.7 | 5.0 | **85.7** |

---

## P03 评分

### Q1 subscription-hub
- 正确性 /45：40 — 自有 string 存储（`subscription_hub.hpp:39-51`），drain `batch.swap`+shared_ptr 快照、锁外投递（`subscription_hub.cpp:39-84`），逐回调 try/catch + delivered 含异常（`subscription_hub.cpp:68-73`），`active` 标志 + 每次回调前短锁检查（`subscription_hub.cpp:54-62`），过期模减（`subscription_hub.cpp:86-102`）。实跑 `edge_checks.cpp` 全过 + 探针全过（重入、unsub-other、异常隔离、回绕、inner drain）。**缺陷**：`delivered` 在 try/catch 之前 `++stats.delivered`（`subscription_hub.cpp:68`）——但仅当 `should_invoke` 为真才递增，语义正确（与“含异常”一致）。主缺陷：存活检查与 topic 检查分散在锁内（`subscription_hub.cpp:55-62`），每次回调前一次短锁+读 `active`/`topic`，正确但比 shared_ptr+atomic 略繁；`unsubscribe` erase 在锁内销毁 callback（`subscription_hub.cpp:25-30`）同前述未验证风险。
- C++质量 /20：17 — shared_ptr 订阅合理；但存活检查需锁（非原子）。
- 约束 /15：15 — API 不变，纯 C++17。
- 可维护 /10：8 — 清晰。
- 验证 /10：8 — 提供 `edge_checks.cpp`（6 项边界）实跑通过；ANSWER.md 简洁。验证充分度中等。
- 小计：88
- 关键缺陷：存活检查持锁（非纯原子），callback 锁内销毁的未验证风险。

### Q2 coalescing-cache
- 正确性 /45：44 — thread-local `active_loads` 栈判同缓存同 key 递归（`coalescing_cache.cpp:20-41,88-91`），loader 与时钟均锁外执行（`coalescing_cache.cpp:140-150`），完成时钟 TTL（`coalescing_cache.cpp:152-164`），失败/异常不缓存，invalidate 设 `cacheable=false`（`coalescing_cache.cpp:193-208`），LRU list+map。实跑 `edge_checks.cpp` 全过 + 探针全过（8 线程合并、跨 key、invalidate-during、递归）。**亮点**：thread_local 加载集合（含 state 指针）比 thread::id 更鲁棒（同线程递归才判 recursive_load，跨线程始终合并），且 `ActiveLoadScope` RAII 清理；时钟异常也处理。轻微：`size()` 在持锁时主动清理过期项并调用 `now()`（`coalescing_cache.cpp:222-231`）——`now()` 是用户函数，理论上不应在锁内执行用户代码（参考解答强调 clock 调用应锁外）。记为设计瑕疵。
- C++质量 /20：18 — RAII scope、异常处理周到；size() 锁内调 now() 略减。
- 约束 /15：15 — API 不变，纯 C++17。
- 可维护 /10：9 — thread_local 递归检测方案优雅。
- 验证 /10：8 — 提供 `edge_checks.cpp`（4 场景）实跑通过。
- 小计：94
- 关键缺陷：`size()` 在 state 锁内调用用户 `now()`（`coalescing_cache.cpp:214,222`），违反“锁内不执行用户代码”。

### Q3 routing-config
- 正确性 /45：45 — `shared_ptr<const Config> current_` 发布、先校验后发布、版本加一（`routing_config.cpp:42-62`），`current()` thread_local pin（`routing_config.cpp:21-24`）跨 reload 有效（实跑 100 次 reload 旧引用 version 不变），观察者用 `shared_ptr<const ObserverEntry>` 存储，reload 锁内只拷贝 shared_ptr 副本（`routing_config.cpp:48-61`）——**正确满足“锁内不复制/销毁 std::function”**（探针 reload 后 copies 不增加、销毁不持锁），锁外解引用通知、异常隔离计数，find_target 单快照最长前缀。实跑 `edge_checks.cpp` 全过 + 探针全过（observer copy/dtor 探针 P03 copies=1 无重锁——确认 shared_ptr 方案）。
- C++质量 /20：19 — shared_ptr<ObserverEntry> 是参考级正确做法。
- 约束 /15：15 — 签名不变，纯 C++17。
- 可维护 /10：9 — 清晰。
- 验证 /10：8 — 提供 `edge_checks.cpp` 实跑通过；验证充分。
- 小计：96
- 关键缺陷：无明显缺陷。Q3 是 P03 最强项。

## P03 汇总
| 题 | 正确性 | C++质量 | 约束 | 可维护 | 验证 | 总分 |
|----|--------|---------|------|--------|------|------|
| Q1 | 40/45 | 17/20 | 15/15 | 8/10 | 8/10 | 88/100 |
| Q2 | 44/45 | 18/20 | 15/15 | 9/10 | 8/10 | 94/100 |
| Q3 | 45/45 | 19/20 | 15/15 | 9/10 | 8/10 | 96/100 |
| 平均 | 43.0 | 18.0 | 15.0 | 8.7 | 8.0 | **92.7** |

---

## P04 评分

### Q1 subscription-hub
- 正确性 /45：43 — 自有 string 存储（`subscription_hub.hpp:40-52`），shared_ptr+`atomic<bool> active` 订阅（`subscription_hub.hpp:45`），drain `batch.swap`+快照、锁外投递（`subscription_hub.cpp:39-79`），每个事件先按 `active`+topic 构造 targets（`subscription_hub.cpp:51-58`），逐回调 try/catch + delivered 含异常，过期模减 + ttl==0（`subscription_hub.cpp:81-101`）。实跑 `edge_checks.cpp` 13 项全过（含递归 drain 处理新发布事件、wraparound 边界、4 publisher+2 drainer 并发 delivered==expected）+ 探针全过。**缺陷**：`unsubscribe` 锁内 erase（`subscription_hub.cpp:20-30`）销毁 callback 同前述风险；tick 更新在每回调后短锁（`subscription_hub.cpp:71-74`）正确但比 P01 略碎。
- C++质量 /20：18 — shared_ptr+atomic 方案与 P01 同级；targets 收集减少锁竞争。
- 约束 /15：15 — API 不变，纯 C++17。
- 可维护 /10：9 — 清晰，文档详尽。
- 验证 /10：10 — `edge_checks.cpp` 覆盖最全（13 项含并发）实跑通过。
- 小计：95
- 关键缺陷：callback 锁内销毁的未验证风险（与同类）。

### Q2 coalescing-cache
- 正确性 /45：42 — `promise<LoadResult>`+`shared_future` 合并（`coalescing_cache.cpp:29-36`），loader 锁外执行（`coalescing_cache.cpp:105-114`），同线程递归 `recursive_load`（`coalescing_cache.cpp:89-91`），完成时钟 TTL（`coalescing_cache.cpp:124-129`），失败/异常不缓存，invalidate 设 `write_to_ready=false`（`coalescing_cache.cpp:148-160`），LRU list+map。实跑 `edge_checks.cpp` 14 项全过 + 探针全过。**缺陷**：`now()` 在 state 锁内调用（`coalescing_cache.cpp:74,125`）用于 expiry 比较/完成时间——参考明确要求 clock 调用锁外（用户函数可重入 API 致死锁）。ANSWER.md 自承“now() 回调在 state 锁内调用……若回调自身重入 get_or_load 会死锁；这是用户契约问题”——但题面语义 2 要求 loader 重入不死锁，clock 虽非 loader 但同属用户注入函数，记为设计缺陷（非致命，clock 通常不重入）。
- C++质量 /20：17 — promise/future 方案正确；锁内调 now() 减分。
- 约束 /15：14 — 锁内执行用户 clock 函数与“loader 在锁外”精神略有出入（非明确禁止，但 borderline）。
- 可维护 /10：9 — 清晰。
- 验证 /10：10 — `edge_checks.cpp` 14 项（含 invalidate-during-joiners-share）实跑通过，验证最全。
- 小计：92
- 关键缺陷：`now()` 在 state 锁内调用（`coalescing_cache.cpp:74`）。

### Q3 routing-config
- 正确性 /45：44 — `shared_ptr<const Config> current_config_` 发布、先校验后发布、版本加一（`routing_config.cpp:41-54`），`current()` 用 `thread_local unordered_map<this, shared_ptr>` pin（`routing_config.cpp:12-23`）跨 reload 有效（实跑 100 次验证），锁外通知 + 异常隔离（错误计数批量累加，`routing_config.cpp:56-68`），find_target 单快照最长前缀。实跑 `edge_checks.cpp` 16 项全过（含并发 reload/snapshot/find_target/current、observer 重入 reload 触发嵌套发布、notify-once）+ 探针全过。**缺陷**：reload 锁内 `to_notify.assign(observers_.begin(),observers_.end())`（`routing_config.cpp:53`）复制 std::function 持锁——同 P01/P02 的未验证风险（探针显示当前不重入故未触发）；`current()` 的 thread_local map 以 `this` 为 key，对象析构后同地址复用会覆盖（ANSWER.md 已说明，行为正确）。
- C++质量 /20：18 — thread_local map pin 方案正确；错误计数批量加锁优化合理。
- 约束 /15：15 — 签名不变，纯 C++17，per-thread 非“全局共享可变路由表”。
- 可维护 /10：9 — 清晰。
- 验证 /10：10 — `edge_checks.cpp` 16 项实跑通过，验证最全。
- 小计：96
- 关键缺陷：observer std::function 锁内复制（未验证风险）。

## P04 汇总
| 题 | 正确性 | C++质量 | 约束 | 可维护 | 验证 | 总分 |
|----|--------|---------|------|--------|------|------|
| Q1 | 43/45 | 18/20 | 15/15 | 9/10 | 10/10 | 95/100 |
| Q2 | 42/45 | 17/20 | 14/15 | 9/10 | 10/10 | 92/100 |
| Q3 | 44/45 | 18/20 | 15/15 | 9/10 | 10/10 | 96/100 |
| 平均 | 43.0 | 17.7 | 14.7 | 9.0 | 10.0 | **94.3** |

---

## P05 评分

### Q1 subscription-hub
- 正确性 /45：38 — 自有 string 存储（`subscription_hub.hpp:39-50`），`active_ids_` unordered_set 跟踪存活（`subscription_hub.hpp:55`），drain swap+快照（拷贝 callback 到 `SubSnap`，`subscription_hub.cpp:31-47`）、锁外投递、逐回调 try/catch + delivered 含异常，过期模减（`subscription_hub.cpp:92-106`）。实跑探针全过（重入、unsub-other、异常、回绕、inner drain）。**缺陷**：`SubSnap` 在 drain 锁内拷贝整份 callback（`subscription_hub.cpp:45`），N 个订阅 N 次 std::function 复制持锁；tick 更新在 drain 末尾一次性锁内对所有 attempted_ids 线性扫描 subscriptions_（`subscription_hub.cpp:77-87`）O(N²)；存活检查每次回调前短锁查 `active_ids_`（`subscription_hub.cpp:56-61`）。`unsubscribe` 锁内 erase 销毁 callback（`subscription_hub.cpp:16-24`）同前述风险。设计正确但效率/封装偏弱。未提供 boundary 测试文件（ANSWER.md 列了覆盖项但 tests/ 下仅 public_checks.cpp）。
- C++质量 /20：15 — 值拷贝订阅 + active_ids 双结构冗余，O(N²) tick 更新。
- 约束 /15：15 — API 不变，纯 C++17。
- 可维护 /10：7 — active_ids 与 subscriptions_ 双维护，易不一致。
- 验证 /10：5 — 仅公开检查通过；ANSWER.md 声称覆盖多项但无自测文件，验证证据薄弱。
- 小计：80
- 关键缺陷：drain 全量拷贝 callback；O(N²) tick 更新；无自测文件。

### Q2 coalescing-cache
- 正确性 /45：42 — `promise`+`shared_future` 合并（`coalescing_cache.cpp:36-41,95-102`），loader 锁外执行（`coalescing_cache.cpp:106-113`），同线程递归 `recursive_load`（`coalescing_cache.cpp:86-89`），完成时钟 TTL（`coalescing_cache.cpp:133-136`），失败/异常不缓存，invalidate 设 `invalidated`（`coalescing_cache.cpp:163-166`），LRU list+map。实跑公开检查通过 + 探针全过（8 线程合并、跨 key、invalidate-during、递归 diff/same）。**缺陷**：`now()` 在 state 锁内调用（`coalescing_cache.cpp:72,135`）——同 P04 的设计缺陷（锁内执行用户 clock）；未提供 boundary 测试文件（ANSWER.md 列覆盖项但 tests/ 仅 public_checks.cpp）；evict 循环 `--back` 手动迭代（`coalescing_cache.cpp:138-143`）可读性差。
- C++质量 /20：16 — 正确；锁内调 now()、手动 list 迭代减分。
- 约束 /15：14 — 锁内执行用户 clock。
- 可维护 /10：7 — 手动迭代、include 未用项（shared_mutex 引入但未用，`coalescing_cache.cpp:7`）。
- 验证 /10：5 — 仅公开检查；无自测文件。
- 小计：84
- 关键缺陷：`now()` 锁内调用；无自测文件。

### Q3 routing-config
- 正确性 /45：42 — `shared_ptr<const Config> current_snapshot_` + `shared_mutex` 读写锁（`routing_config.hpp:45-46`），先校验后发布、版本加一（`routing_config.cpp:33-51`），`current()` thread_local pin（`routing_config.cpp:10-15`）跨 reload 有效（实跑验证），锁外通知 + 异常隔离，find_target shared_lock 单快照最长前缀。实跑探针全过（current lifetime、observer 重入 a=1 b=1）。**缺陷**：reload 持 `observer_mutex_` + `config_rwlock_` 写锁时拷贝 `observers_` map（复制 std::function）持锁（`routing_config.cpp:50`）同未验证风险；`shared_mutex` 在 g++ 9 下读写锁是合理等价实现（接受）；未提供 boundary 测试文件（ANSWER.md 列覆盖项但无自测）。
- C++质量 /20：17 — shared_mutex 方案可接受；双锁顺序需谨慎。
- 约束 /15：15 — 签名不变，纯 C++17。
- 可维护 /10：8 — 清晰。
- 验证 /10：5 — 仅公开检查；无自测文件。
- 小计：87
- 关键缺陷：observer std::function 锁内复制；无自测文件。

## P05 汇总
| 题 | 正确性 | C++质量 | 约束 | 可维护 | 验证 | 总分 |
|----|--------|---------|------|--------|------|------|
| Q1 | 38/45 | 15/20 | 15/15 | 7/10 | 5/10 | 80/100 |
| Q2 | 42/45 | 16/20 | 14/15 | 7/10 | 5/10 | 84/100 |
| Q3 | 42/45 | 17/20 | 15/15 | 8/10 | 5/10 | 87/100 |
| 平均 | 40.7 | 16.0 | 14.7 | 7.3 | 5.0 | **83.7** |

---

## 所有匿名 ID 总分对比

| ID | Q1 | Q2 | Q3 | 平均 |
|----|----|----|----|------|
| P01 | 96 | 99 | 97 | **97.3** |
| P02 | 81 | 88 | 88 | **85.7** |
| P03 | 88 | 94 | 96 | **92.7** |
| P04 | 95 | 92 | 96 | **94.3** |
| P05 | 80 | 84 | 87 | **83.7** |

排名：P01 (97.3) > P04 (94.3) > P03 (92.7) > P02 (85.7) > P05 (83.7)。

## 匿名结论

- **实现正确性高度趋同，分差主要来自“验证证据 + 锁内用户代码”两点。** 五个对象在正常路径与常规并发/重入/回绕场景下均通过本评分者的探针实跑（合并单 loader、递归 recursive_load、invalidate-during-load、跨 reload 引用存活、observer 重入不死锁全部一致通过）。因此正确性维度差距较小（Q1 38–45、Q2 42–45、Q3 42–45），真正拉开分数的是验证维度的 10 分（是否有可信自测）与 C++ 质量/约束里“是否在锁内执行用户函数（callback 销毁、observer std::function 复制、clock 调用）”。
- **Q1（组合 Bugfix）**：P01/P04 最强（shared_ptr+atomic 订阅保活 + 自测最全）；P02/P05 用值拷贝订阅列表、drain 全量复制 callback 且无自测，明显落后；P03 居中。
- **Q2（并发合并缓存）**：P01 最强（per-key InFlight 独立锁、loader/clock 全锁外、自测 8 项）；P03 次之（thread_local 递归检测优雅，但 size() 锁内调 now()）；P04/P05 在 state 锁内调用用户 `now()`（与“loader 锁外”精神冲突），P05 无自测。
- **Q3（安全发布重构）**：P03 最强（唯一用 `shared_ptr<const ObserverEntry>` 存储、reload 锁内只拷贝指针，严格满足“锁内不复制/销毁 std::function”）；P01/P04 紧随（thread_local pin 正确、自测全）；P02/P05 reload 锁内复制 observers_ map 的 std::function 且无自测。
- **最高置信度差异点（跨对象区分度最大）**：
  1. **是否提供并实跑可信的自测** —— P01/P03/P04 各题均带 boundary/edge_checks 且实跑通过（验证 8–10）；P02/P05 全部三题仅依赖公开检查、无自测文件（验证 5）。这是 P02/P05 与第一梯队 ~9 分差距的主因，证据确凿（tests/ 目录文件清单 + ANSWER.md 自承）。
  2. **Q3 observer 是否用 shared_ptr 封装、避免锁内复制 std::function** —— 仅 P03 做到（`routing_config.hpp:45` shared_ptr<const ObserverEntry>，reload 锁内只拷贝指针 `routing_config.cpp:57-60`），探针 reload 后 copies 不增加；P01/P02/P04/P05 均在 reload/subscribe 锁内复制或销毁 std::function（回调析构重入 API 的极端场景为未验证死锁风险）。这是 Q3 内 P03 领先的硬性技术差异。
  3. **Q2 是否在 state 锁内调用用户 clock** —— P01/P03 将 clock 调用置于锁外；P04（`coalescing_cache.cpp:74,125`）与 P05（`coalescing_cache.cpp:72,135`）在锁内调用，P04 自承若 clock 重入会死锁。这是 Q2 内 P01/P03 领先的可验证差异。
