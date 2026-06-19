# 盲评报告（scorer-B）

评测对象：P01、P02、P03、P04、P05（匿名）。仅依据 `blind-package/` 下源码与 `scorer-reference/cpp17-advanced-v1.md`。

环境：g++ 9.4.0，`-std=c++17 -Wall -Wextra -Wpedantic -pthread`。每个 `Pxx/qXX` 均实跑 `./run_public_checks.sh`（5×3=15 全部 rc=0），并额外编译运行参评者自带边界测试与我编写的针对性 probe（见每题"验证"）。

通用评分基准：所有题面功能/并发/边界组合 probe 通过；额外发现的并发缺陷按"未验证风险/伪修复"在相应维度扣分。

---

## P01 评分

### Q1 subscription-hub
- 正确性 /45：42 — 语义 1-7 全覆盖：`SubscriptionData`/`QueuedEvent` 自有 `std::string` 存储（`subscription_hub.hpp:44,57-58`）；drain 在锁内 swap 队列+拷贝订阅快照后锁外投递（`src:59-67`）；`alive` 原子标志实现"取消只阻后续事件"（`src:36-47,76`）；逐回调 try/catch，`delivered` 含异常、`callback_errors` 单列（`src:86-94`）；`elapsed = now - last` 模减 + ttl==0 短路（`src:13-15,108`）。我的 probe（临时串/重入publish/重入drain不重投/取消/异常/回绕/ttl0/8线程并发）3 次全过。
- C++质量 /20：18 — shared_ptr 保活、atomic last_active_tick/alive、memory_order 合理；无明显 UB。
- 约束 /15：15 — 公共 API 未变，私有结构合理调整。
- 可维护 /10：9 — 注释清晰、职责分明。
- 验证 /10：9 — 自带 `boundary_checks.cpp` 覆盖全面，ANSWER 处理顺序说明到位，实跑通过。
- 小计：93
- 关键缺陷：`unsubscribe`/`expire_idle` 在锁内 `erase`，若该 shared_ptr 是最后一个引用，`std::function`（及用户捕获）析构发生在 `mutex_` 内（`src:41-46,116`）；参考明确要求销毁用户 callable 不可在 mutex 内。普通 lambda 不触发，但析构重入 hub 会死锁（我的 dtor-reenter probe 验证为 timeout）。属真实但边缘缺陷。

### Q2 coalescing-cache
- 正确性 /45：44 — 完整状态机：ready 命中刷 LRU（`src:75-84`）；per-key `shared_ptr<InFlight>` 自带 mutex+cv，loader 在 state 锁外执行（`src:107-123`）；`loader_thread` 判同 key 递归返回 `recursive_load`（`src:93-97`）；完成时时钟写 ready + ttl/capacity 判定（`src:125-153`）；`discard_result` 处理 in-flight invalidate（`src:166-179`）；LRU list+map、capacity/ttl==0 正确。我的 probe（合并/不同key/递归/抛异常/ttl0/invalidate-in-flight/LRU/stress）每参评者 3 轮全过。
- C++质量 /20：19 — 双锁分层清晰，异常转 failed 不跨边界。
- 约束 /15：15 — 公共 API/枚举/值类型不变。
- 可维护 /10：9 — 注释充分。
- 验证 /10：9 — 自带 `boundary_checks.cpp` 覆盖 invalidate 竞态等，实跑通过。
- 小计：96
- 关键缺陷：无明显。`std::thread::id` 作 loader 身份在极异构造场景下理论可误判，题面未要求。

### Q3 routing-config
- 正确性 /45：41 — 全部功能 probe 通过：valid 失败不变/版本严格+1（`src:49-62`）；`thread_local shared_ptr` pin 保活 current()（`src:17-27`）；锁外通知 + 异常计数（`src:72-79`）；最长 prefix（`src:103-109`）；并发 reader/writer 版本到 800。但 reload 锁内 `to_notify.emplace_back(..., kv.second)`（`src:64-67`）在 `mutex_` 下**复制了用户 `std::function`**，违反"不得在 mutex 内复制 std::function"；我的 copy-reenter probe（observer 复制构造重入 snapshot）对 P01 死锁（timeout 124）。dtor-reenter 同样死锁。
- C++质量 /20：18 — 重构干净，无 UB。
- 约束 /15：15 — 公共 API 不变。
- 可维护 /10：9 — 结构清晰。
- 验证 /10：9 — 自带 `boundary_checks.cpp`（含跨 reload 引用存活、并发）实跑通过。
- 小计：92
- 关键缺陷：观察者 `std::function` 的复制发生在 reload 持锁段（`src:65-67`），与参考"observer 复制重入配置 API 不死锁"相悖；copy-reenter/dtor-reenter probe 均死锁。属真实缺陷。

### P01 汇总
| 题 | 正确性 | C++质量 | 约束 | 可维护 | 验证 | 总分 |
|----|--------|---------|------|--------|------|------|
| Q1 | 42/45 | 18/20 | 15/15 | 9/10 | 9/10 | 93/100 |
| Q2 | 44/45 | 19/20 | 15/15 | 9/10 | 9/10 | 96/100 |
| Q3 | 41/45 | 18/20 | 15/15 | 9/10 | 9/10 | 92/100 |
| 平均 | | | | | | 93.7 |

---

## P02 评分

### Q1 subscription-hub
- 正确性 /45：40 — 功能基本正确：自有 string 存储（`subscription_hub.cpp:11,26`）；drain swap+snapshot 后锁外投递（`src:32-38`）；逐回调短锁查存活再调用（`src:50-66`）；try/catch 隔离异常（`src:62-67`）；模减 elapsed（`src:93-95`）。我的 8-probe 全过。但 `expire_idle` 直接 `remove_if` 删除 `Subscription`（`src:90-99`），**last-ref 的 std::function 在 mutex 内析构**；dtor-reenter probe 死锁。
- C++质量 /20：17 — 结构简单合理；drain 对每个(event,sub)重复短锁扫描 O(N·M)，略低效但正确。
- 约束 /15：15 — API 不变。
- 可维护 /10：8 — 逻辑清晰但多次短锁略碎。
- 验证 /10：6 — **未新增任何边界测试**（ANSWER 明确承认"未新增边界压力测试，依赖验收测试覆盖"）。仅靠 public_checks。正确性靠我的 probe 背书，扣验证分。
- 小计：86
- 关键缺陷：无自带并发/边界测试；last-ref 析构在锁内（同 P01）。

### Q2 coalescing-cache
- 正确性 /45：43 — 状态机正确：per-key `shared_ptr<InFlight>` + promise-style result（`src:29-36`）；loader 锁外执行（`src:154-163`）；done 标志 + inflight_mtx 等待（`src:147-152`）；写 ready 前 set done/notify、写 ready 检查 invalidated（`src:168-186`）；invalidate 置 invalidated（`src:191-202`）；LRU list+map。我的 probe 全过。**但**：owner 先 `set done=true`（`src:172`）**再**写 ready（`src:178-185`），存在窗口：新调用者看到 done 直接返回 result（`src:132-135`）而 owner 尚未写 ready——语义仍自洽（同结果），无功能 bug。
- C++质量 /20：18 — 锁顺序一致 state->mtx→inflight_mtx，无死锁。
- 约束 /15：15 — API 不变。
- 可维护 /10：8 — 注释中英混杂但清楚。
- 验证 /10：6 — **未自带边界测试**（ANSWER 承认）。仅 public_checks。靠我的 probe 背书。
- 小计：90
- 关键缺陷：无自带边界测试，验证证据薄弱；otherwise 正确。

### Q3 routing-config
- 正确性 /45：40 — 功能全对：valid 失败不变（`routing_config.cpp:38-41`）；thread_local pin（`src:14-19`）；锁外通知 + 异常计数（`src:55-62`）；最长 prefix（`src:87-93`）；并发到 800。但 reload 锁内 `observers_snapshot = observers_`（`src:50`）**在 mutex 下复制 std::function**；copy-reenter/dtor-reenter probe 均死锁。
- C++质量 /20：18 — 干净。
- 约束 /15：15 — API 不变。
- 可维护 /10：8 — 清晰。
- 验证 /10：6 — **无自带边界测试**（ANSWER 承认），仅 public_checks。
- 小计：87
- 关键缺陷：observer 复制在锁内（`src:50`）；无自带边界测试。

### P02 汇总
| 题 | 正确性 | C++质量 | 约束 | 可维护 | 验证 | 总分 |
|----|--------|---------|------|--------|------|------|
| Q1 | 40/45 | 17/20 | 15/15 | 8/10 | 6/10 | 86/100 |
| Q2 | 43/45 | 18/20 | 15/15 | 8/10 | 6/10 | 90/100 |
| Q3 | 40/45 | 18/20 | 15/15 | 8/10 | 6/10 | 87/100 |
| 平均 | | | | | | 87.7 |

---

## P03 评分

### Q1 subscription-hub
- 正确性 /45：42 — 功能正确：shared_ptr<Subscription> + active 标志（`subscription_hub.cpp:9-16,20-30`）；drain swap+snapshot 后锁外，每次投递前短锁查 active（`src:39-66`）；try/catch（`src:69-73`）；模减 elapsed（`src:91-92`）。我的 8-probe 全过。`expire_idle` 锁内 erase 删 shared_ptr（`src:88-101`）→ last-ref 析构在锁内，dtor-reenter probe 死锁。
- C++质量 /20：18 — 合理。
- 约束 /15：15 — API 不变。
- 可维护 /10：9 — 简洁。
- 验证 /10：9 — 自带 `edge_checks.cpp`（临时串/重入publish/取消/异常/回绕），实跑通过。
- 小计：93
- 关键缺陷：last-ref 析构在锁内（同 P01 类别）；dtor-reenter probe 死锁。

### Q2 coalescing-cache
- 正确性 /45：44 — thread_local 加载栈判同缓存同 key 递归（`coalescing_cache.cpp:18-41,89-91`），比 thread_id 更精确（按 state 实例）；loader+clock 均锁外（`src:95-150`）；完成时时钟写 ready（`src:152-180`）；`cacheable` 标志处理 invalidate（`src:193-208`）；LRU 正确。我的 probe 全过。`size()` 顺带淘汰过期项（`src:222-231`）——副作用但 size 仍正确，可接受。
- C++质量 /20：19 — thread_local 栈 + RAII scope 清理优雅。
- 约束 /15：15 — API 不变。
- 可维护 /10：9 — 清晰。
- 验证 /10：9 — 自带 `edge_checks.cpp` 实跑通过。
- 小计：96
- 关键缺陷：无重大。clock 抛异常被吞不影响交付，合理。

### Q3 routing-config
- 正确性 /45：43 — 功能全对；**且 observer 用 `shared_ptr<const ObserverEntry>` 存储，reload 在锁内只拷贝 shared_ptr（`routing_config.cpp:57-60,64`），不在 mutex 下复制 std::function**——这是 5 份中唯一 copy-reenter probe 通过的。但 `unsubscribe`/`subscribe` 锁内 erase/emplace 仍可能析构 last-ref（`src:84-87,75-82`），dtor-reenter probe 死锁。
- C++质量 /20：19 — 设计最贴近参考（observer 延迟销毁思路正确）。
- 约束 /15：15 — API 不变。
- 可维护 /10：9 — 清晰。
- 验证 /10：9 — 自带 `edge_checks.cpp` 实跑通过；copy-reenter 验证通过是加分项。
- 小计：95
- 关键缺陷：unsubscribe/subscribe 锁内析构 last-ref std::function（`src:86,80`），dtor-reenter 死锁。copy 路径已正确规避。

### P03 汇总
| 题 | 正确性 | C++质量 | 约束 | 可维护 | 验证 | 总分 |
|----|--------|---------|------|--------|------|------|
| Q1 | 42/45 | 18/20 | 15/15 | 9/10 | 9/10 | 93/100 |
| Q2 | 44/45 | 19/20 | 15/15 | 9/10 | 9/10 | 96/100 |
| Q3 | 43/45 | 19/20 | 15/15 | 9/10 | 9/10 | 95/100 |
| 平均 | | | | | | 94.7 |

---

## P04 评分

### Q1 subscription-hub
- 正确性 /45：42 — 功能正确：shared_ptr<Subscription> + atomic active（`subscription_hub.cpp:10-16`）；drain swap+snapshot，每事件锁外建 targets（`src:39-64`）；try/catch（`src:65-69`）；ttl==0 短路 + 模减（`src:84-90`）。我的 8-probe 全过。unsubscribe 锁内 erase shared_ptr（`src:26-29`）→ last-ref 析构在锁内，dtor-reenter probe 死锁。
- C++质量 /20：18 — 合理。
- 约束 /15：15 — API 不变。
- 可维护 /10：9 — 注释详尽，处理顺序说明清楚。
- 验证 /10：9 — 自带 `edge_checks.cpp` 极为全面（13 个用例含并发、回绕、tick 刷新），实跑通过。
- 小计：93
- 关键缺陷：last-ref 析构在锁内；dtor-reenter probe 死锁。

### Q2 coalescing-cache
- 正确性 /45：43 — `promise/shared_future` 合并（`coalescing_cache.cpp:29-36`）；loader 锁外（`src:106-114`）；thread_id 判递归（`src:89-91`）；`write_to_ready` 处理 invalidate（`src:122-134,156-159`）；出锁后 set_value 避免锁内唤醒（`src:138-143`）；LRU 正确。我的 probe 全过。**但**：`s.now()` 在 `s.mutex` 内调用（`src:74`），若 clock 回调重入 get_or_load 会死锁——ANSWER 自承，题面未要求 clock 重入，记为边缘风险。
- C++质量 /20：18 — shared_future 用法正确，保活 inflight_holder 防 broken_promise。
- 约束 /15：15 — API 不变。
- 可维护 /10：9 — 注释详尽。
- 验证 /10：9 — 自带 `edge_checks.cpp`（13 用例，含 invalidate joiner 共享、不同key并发），实跑通过。
- 小计：94
- 关键缺陷：`now()` 在 state 锁内调用（`src:74`），clock 重入会死锁（边缘，ANSER 自承）。

### Q3 routing-config
- 正确性 /45：40 — 功能全对：valid 失败不变/版本严格+1（`routing_config.cpp:42-53`）；**per-instance thread_local pin（unordered_map<this,shared_ptr>，`src:13-22`）**——比单槽 pin 更稳健（多实例不串扰）；锁外通知 + 异常计数（`src:57-68`）；最长 prefix；并发到 800。但 reload 锁内 `to_notify.assign(observers_.begin(), observers_.end())`（`src:53`）**复制 std::function**；copy-reenter/dtor-reenter probe 均死锁。
- C++质量 /20：18 — pin 设计细致。
- 约束 /15：15 — API 不变。
- 可维护 /10：9 — 详尽。
- 验证 /10：9 — 自带 `edge_checks.cpp`（16 用例），实跑通过。
- 小计：90
- 关键缺陷：observer 复制在锁内（`src:53`）；copy-reenter/dtor-reenter 死锁。

### P04 汇总
| 题 | 正确性 | C++质量 | 约束 | 可维护 | 验证 | 总分 |
|----|--------|---------|------|--------|------|------|
| Q1 | 42/45 | 18/20 | 15/15 | 9/10 | 9/10 | 93/100 |
| Q2 | 43/45 | 18/20 | 15/15 | 9/10 | 9/10 | 94/100 |
| Q3 | 40/45 | 18/20 | 15/15 | 9/10 | 9/10 | 90/100 |
| 平均 | | | | | | 92.3 |

---

## P05 评分

### Q1 subscription-hub
- 正确性 /45：39 — 功能正确：自有 string + `active_ids_` set（`subscription_hub.cpp:11,16-23`）；drain swap+snapshot，每次投递前短锁查 active_ids（`src:52-61`）；try/catch（`src:67-71`）；模减 elapsed（`src:96-97`）。我的 8-probe 全过。**但**：`last_active_tick` 在 drain **结束时批量更新**（`src:77-87`），违反语义 6"每次实际尝试回调后更新活跃 tick"——长 drain 中并发 expire_idle 可能误删正被投递的订阅。`active_ids_` 用 unordered_set 查找，O(1)。unsubscribe 锁内 erase Subscription（值类型，`src:19-24`）→ last-ref 析构在锁内，dtor-reenter 死锁。
- C++质量 /20：17 — 基本合理；批量 tick 偏离语义。
- 约束 /15：15 — API 不变。
- 可维护 /10：8 — 清晰。
- 验证 /10：6 — **无自带边界测试文件**（tests/ 仅 public_checks.cpp），ANSWER 列了"新增测试覆盖"但实际未提供文件；属声称与事实不符的减分项。靠我的 probe 背书。
- 小计：85
- 关键缺陷：tick 批量更新偏离语义 6；无自带边界测试文件（ANSWER 列举但 tests/ 不存在）；last-ref 析构在锁内。

### Q2 coalescing-cache
- 正确性 /45：43 — promise/shared_future 合并（`coalescing_cache.cpp:36-43,95-102`）；loader 锁外（`src:106-113`）；thread_id 判递归（`src:86-89`）；invalidated 处理（`src:124,133-134,164-166`）；出锁后 set_value（`src:126-129` 在锁内 set_value！再 unlock 再 notify——`src:148-149`）；LRU 正确。我的 probe 全过。**但**：`entry.promise->set_value(result)` 在 `state_->mutex` 持有期间执行（`src:115-129`），set_value 会唤醒等待者，等待者被 future.get 阻塞不持锁，故不会死锁，但在锁内唤醒导致被唤醒线程立即争锁，略增竞争；非功能 bug。
- C++质量 /20：18 — 合理。包含未使用的 `<shared_mutex>` 头（`src:7`）——小瑕疵。
- 约束 /15：15 — API 不变。
- 可维护 /10：8 — 清晰。
- 验证 /10：6 — **无自带边界测试文件**（tests/ 仅 public_checks.cpp），ANSWER 同样列了"新增测试覆盖"但未提供文件；声称与事实不符。靠我的 probe 背书。
- 小计：90
- 关键缺陷：set_value 在 state 锁内（`src:127`）轻微竞争；无自带边界测试文件（ANSWER 列举但不存在）。

### Q3 routing-config
- 正确性 /45：41 — 功能全对：读写锁 `config_rwlock_` + observer 独立锁（`routing_config.hpp:40-46`）；valid 失败不变（`src:34-36`）；thread_local pin（`src:11-15`）；锁外通知 + 异常计数（`src:53-60`）；最长 prefix；并发到 800。**`unsubscribe`/`subscribe` 在独立 observer_mutex_ 下操作，析构 std::function 不碰 config_rwlock_，dtor-reenter probe 通过（唯一通过者）**。但 reload 同时持 `observer_mutex_`+`config_rwlock_`（`src:42-43`），锁内 `observer_snap = observers_`（`src:50`）**复制 std::function**；copy-reenter probe 中观察者复制构造重入 `snapshot()`（需 shared_lock config_rwlock_）与 reload 持有的 unique_lock 冲突 → 抛 `std::system_error(EDEADLK)` 并 **abort**（我的 probe 验证：terminate）。
- C++质量 /20：17 — 读写锁分离思路好，但 copy 路径在锁内导致 EDEADLK 崩溃。
- 约束 /15：15 — API 不变。
- 可维护 /10：8 — 清晰。
- 验证 /10：6 — **无自带边界测试文件**（tests/ 仅 public_checks.cpp），ANSWER 列举但未提供；声称与事实不符。
- 小计：87
- 关键缺陷：observer 复制在锁内（`src:50`）触发 EDEADLK 崩溃（copy-reenter probe terminate）；无自带边界测试文件。

### P05 汇总
| 题 | 正确性 | C++质量 | 约束 | 可维护 | 验证 | 总分 |
|----|--------|---------|------|--------|------|------|
| Q1 | 39/45 | 17/20 | 15/15 | 8/10 | 6/10 | 85/100 |
| Q2 | 43/45 | 18/20 | 15/15 | 8/10 | 6/10 | 90/100 |
| Q3 | 41/45 | 17/20 | 15/15 | 8/10 | 6/10 | 87/100 |
| 平均 | | | | | | 87.3 |

---

## 总分对比表

| 匿名ID | Q1 | Q2 | Q3 | 平均 |
|--------|----|----|----|------|
| P01 | 93 | 96 | 92 | 93.7 |
| P02 | 86 | 90 | 87 | 87.7 |
| P03 | 93 | 96 | 95 | 94.7 |
| P04 | 93 | 94 | 90 | 92.3 |
| P05 | 85 | 90 | 87 | 87.3 |

---

## 匿名结论

**整体梯度**：P03 ≈ P01 > P04 > P02 ≈ P05。分差量级：最高（P03 94.7）与最低（P05 87.3）相差约 7 分；中位 P04 92.3。

**最强项识别（最高置信度差异点）**：
1. **Q3 观察者 `std::function` 锁内复制重入**：这是区分度最高的点。仅 P03 把观察者包成 `shared_ptr<const ObserverEntry>`，reload 锁内只拷贝指针、不在 mutex 下复制/析构 `std::function`，copy-reenter probe 唯一通过；P01/P02/P04 在锁内直接拷贝 observer map → 复制重入死锁；P05 用读写锁分离但复制重入触发 EDEADLK 直接 abort。这是参考明确点名"observer 复制或析构重入配置 API 不死锁"的硬性语义。
2. **自带验证证据**：P01/P03/P04 三题均提供实际边界测试文件并实跑通过；P02 三题均无自带边界测试（ANSER 承认依赖验收）；P05 三题 ANSWER 列举"新增测试覆盖"但 `tests/` 下实际只有 `public_checks.cpp`（声称与文件事实不符），按 rubric 验证维度显著扣分。
3. **Q1 共性边缘缺陷**：5 份均在 `unsubscribe`/`expire_idle` 锁内 erase，导致"最后一个 `std::function` 引用"在 mutex 内析构（参考点名的同类伪修复）；普通 lambda 不触发，dtor-reenter probe 下 5 份全部死锁——这是全场的共同短板，未据此大幅拉开分差，仅在 C++质量/正确性内统一轻扣。
4. **P05 额外缺陷**：Q1 tick 批量更新（偏离语义 6）、Q2 set_value 在锁内、Q3 EDEADLK 崩溃，整体质量与并发审慎度弱于其余。

**任务类型强弱**：Q2（实现题）5 份均较扎实（90-96），差距最小；Q1/Q3（并发重构/语义题）拉出明显梯度，差距主要来自观察者锁边界处理与验证证据完整度。
