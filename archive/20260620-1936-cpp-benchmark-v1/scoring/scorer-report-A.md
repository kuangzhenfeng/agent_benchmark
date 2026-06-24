# 盲评评分报告 A（scorer-report-A）

评测对象：P01、P02、P03、P04、P05、P06（匿名）。题源：`benchmark-v1`，三题各 100 分，满分 300。
编译/运行环境：g++ 9.4.0、`-std=c++17 -Wall -Wextra -Wpedantic -pthread`，Q3 跨实例检查额外带 AddressSanitizer。公开检查脚本每个测试自带 `timeout 5s`，退出码 `0`=通过、`124`=超时（死锁）、其它=断言/ASAN 失败。
评分依据：以各题 `QUESTION.md` 与 `scorer-reference/benchmark-v1.md` 验收矩阵为准，结合源码事实与公开检查结果；接受任何满足公开语义的等价实现，不以"未采用参考数据结构"扣分。

评分维度：正确性 45 / C++ 质量 20 / 约束遵循 15 / 可维护性 10 / 验证证据 10。

---

## P01

### Q1 subscription-hub：正确性 37 | C++质量 16 | 约束 14 | 可维护 8 | 验证 8 | 总分 83
- 公开检查：`public_checks` + `callable_lifetime_checks` 均 pass，3 次稳定 0/0/0。
- 源码事实：`Subscription::topic`、`QueuedEvent::topic/payload` 改为 `std::string`（自有存储，消除悬空）；`drain` 锁内截取 `pending_` 并 clear、快照订阅列表后解锁逐回调投递（批次语义正确）；新增 `active_ids_` 集合，每事件锁内复查活跃性，回调中 unsubscribe/subscribe 不污染当前批次；`unsubscribe`/`expire_idle` 将 callback `std::move` 到局部 `dying` 后解锁再析构（**销毁在锁外**，callable_lifetime 不死锁）；`expire_idle` 用 `uint32_t elapsed = now_tick - last_active_tick; elapsed >= ttl`（模减正确，ttl=0 立即过期）；每回调 try/catch，`delivered` 计所有尝试、`callback_errors` 单计异常。
- 风险/扣分点：`drain` 第 45 行 `subs = subscriptions_` **在锁内拷贝整个 Subscription 向量（含 `Callback`/`std::function`）**，这违反第 7 条"复制/移动/销毁必须在锁外"——若用户 callable 的拷贝构造重入 hub 将死锁。该路径未被公开测试覆盖（公开测试只覆盖析构重入），故未触发硬封顶，但属于验收矩阵"callback 的复制或析构重入 hub"未处理项，正确性与 C++ 质量据此扣分。ANSWER 误把 Q3 的 `pinned_`/`observer_errors_` 文案拷进 Q1（小瑕疵）。
- ANSWER.md：修改摘要、处理顺序（含 ASCII 流程）、验证命令、剩余风险齐全；未单独新增边界测试文件；未点出 drain 拷贝 callback 的锁边界风险。

### Q2 coalescing-cache：正确性 43 | C++质量 18 | 约束 15 | 可维护 9 | 验证 9 | 总分 94
- 公开检查：`public_checks`（6 线程同 key 合并）+ `clock_reentrancy_checks` 均 pass，3 次稳定 0/0/0。
- 源码事实：`shared_ptr<State>` PIMPL；`thread_local` 加载集合做递归检测；Flight 自带 mutex+cv，owner 在 `state->mtx` 外调用 loader 与 Clock（cache 检查、插入两次 Clock 均锁外）；owner 完成时在同一同步关系下写结果、定 ready 决定、`notify_all`、再从 in-flight 移除；`invalidate` 锁内删 ready 并对在途 flight 置 `should_cache=false`（不取消 loader，结果仍返回但不回写）；LRU `list+unordered_map`，容量 0 不缓存，在途不计容量；loader 异常捕获转 `loader_failed`。
- 风险/扣分点：递归加载集合为 `static thread_local`（进程级、跨实例共享），同线程同 key 但跨实例的调用会误判 `recursive_load`（未被测试覆盖的边界）；ANSWER 自述已意识到 thread_local 在 loader 抛异常时回滚。
- ANSWER.md：状态机、锁边界、invalidate 竞态、LRU、唤醒顺序均说明，含验证命令与输出；未新增边界测试文件。

### Q3 routing-config：正确性 38 | C++质量 14 | 约束 13 | 可维护 8 | 验证 8 | 总分 81
- 公开检查：`public_checks` + `cross_instance_current_checks`(ASAN) + `observer_copy_reentrancy_checks` 5 次均 0；单测 `cross_instance` no-ASAN 8/8 通过，带 ASAN 在 5s 紧超时下偶发 124（~1/8，调度/超时相关，非逻辑死锁）。
- 源码事实：`published_` 为 `shared_ptr<const Config>`，reload 锁内 validate→version+1→构造新快照→原子替换→拷贝观察者 `shared_ptr` 列表，解锁后逐个通知；`current()` 用成员 `pinned_` (`map<thread::id, shared_ptr>`) 做**实例级 per-thread pin**（跨实例正确隔离）；`find_target` 经 `snapshot()` 最长前缀匹配；`unsubscribe` 锁内移出 shared_ptr、锁外析构。
- 风险/扣分点：(1) reload 通知循环第 52 行 `(*kv.second)(published_)` **在锁外读取成员 `published_`**，与并发 reload 的锁内写入构成 `shared_ptr` 数据竞争（C++17 非 atomic，TSan 会报；ASAN 难以捕获）。应改为通知局部 `new_snapshot`。(2) `subscribe` 第 69 行 `make_shared<Observer>(std::move(observer))` **在锁内移动 `Observer`**，违反第 4 条"移动必须在锁外"（公开测试只覆盖 reload 中的拷贝重入，未覆盖 subscribe 的移动重入）。两项均为验收矩阵中未由公开测试覆盖的并发/锁边界缺口，正确性与 C++ 质量据此扣分。
- ANSWER.md：发布点、current() 引用保活（实例级 pin）、观察者快照时刻、异常隔离说明完整；准确点出 `pinned_` map 增长风险；未提及上述 `published_` 锁外读竞争与 subscribe 锁内移动问题。

**P01 三题合计：83 + 94 + 81 = 258 / 300**

---

## P02

### Q1 subscription-hub：正确性 44 | C++质量 19 | 约束 15 | 可维护 9 | 验证 10 | 总分 97
- 公开检查：3 次稳定 0/0/0。
- 源码事实：私有字段 `Callback` 改为 `shared_ptr<Callback>`（subscribe 锁外 `make_shared`，锁内只移动 shared_ptr）；`drain` 锁内快照事件并用 `max_original_id` 过滤掉回调中新订阅者（新 ID > max_original_id 不进本批次），锁外逐事件逐订阅者调用；`unsubscribe`/`expire_idle` 锁内移出 shared_ptr、锁外析构；`expire_idle` 用 `static_cast<uint32_t>(now - last) >= ttl`（模减、ttl=0 立即过期）；异常隔离、`delivered`/`callback_errors` 计数正确。复制/移动/销毁均不在锁内，符合第 7 条。
- 风险/扣分点：drain 每事件多次短锁（取 targets + 更新 tick），性能非最优，ANSWER 自述。
- ANSWER.md：缺陷定位、修复策略、处理顺序、验证命令、`edge_case_checks.cpp`（临时 string_view 生命周期、回调重入 unsubscribe/publish/subscribe、异常隔离、uint32 回绕、TTL=0、多线程）齐全；验证证据满分。

### Q2 coalescing-cache：正确性 44 | C++质量 18 | 约束 15 | 可维护 9 | 验证 10 | 总分 96
- 公开检查：3 次稳定 0/0/0。
- 源码事实：单一 `state_->mutex`+cv 同步关系；`owner_thread = this_thread::get_id()`，同 key 重入时检测到 owner==当前线程立即返回 `recursive_load`；Clock 在取锁前调用（锁外），loader 全程锁外；`invalidated` 标志由同一 mutex 保护；owner 完成时 `should_cache = ok && ttl>0 && capacity>0 && !invalidated`；LRU 用 `lru_seq` 序号、淘汰扫最小序号；容量 0 不缓存但仍合并。
- 风险/扣分点：LRU 淘汰为 O(n) 扫描（ANSWER 自述为性能风险，不影响正确性）。
- ANSWER.md：状态机、锁边界、owner/唤醒顺序、invalidate 竞态、LRU 说明完整 + `edge_case_checks.cpp`（loader 异常/失败、TTL 过期重载、invalidate 阻止回写、递归 recursive_load、容量 0、LRU 淘汰、并发异常）。

### Q3 routing-config：正确性 44 | C++质量 19 | 约束 15 | 可维护 9 | 验证 10 | 总分 97
- 公开检查：5 次 0；`cross_instance` ASAN 在 10 次中 1 次 124（5s 紧超时调度相关），no-ASAN 10/10 通过；observer_copy 0。
- 源码事实：`current_snapshot_` 为 `shared_ptr<const Config>`；`current()` 用 `thread_local unordered_map<const RoutingConfig*, shared_ptr>` 以**实例指针为 key**做 per-instance per-thread pin（跨实例正确隔离，版本不同才更新 pin）；reload 用**局部 `new_snapshot`** 通知（无 `published_` 锁外读竞争）；`subscribe` 锁外 `make_shared` 后锁内移动；`unsubscribe` 锁内移出、锁外析构；观察者存 `shared_ptr<Observer>`，reload 锁内只拷贝 shared_ptr；`find_target` 经 `snapshot()` 最长前缀；异常锁外捕获、锁内累加错误数。
- 风险/扣分点：仅 ASAN 紧超时下偶发 124（调度相关），正确性与质量基本无瑕疵；设计最贴近参考且无数据竞争/锁边界缺口。
- ANSWER.md：发布点、current() 跨实例 pin、回调隔离、发布与通知时刻、剩余风险（裸指针 key、单一 mutex）说明完整 + `edge_case_checks.cpp`。

**P02 三题合计：97 + 96 + 97 = 290 / 300**

---

## P03

### Q1 subscription-hub：正确性 10 | C++质量 8 | 约束 10 | 可维护 6 | 验证 4 | 总分 39（硬封顶 ≤60）
- 公开检查：3 次稳定 124/124/124（`callable_lifetime_checks` 死锁）。
- 源码事实：`drain` 第 46 行 `subscriptions = subscriptions_` **锁内拷贝** Subscription（含 Callback）；`unsubscribe` 第 25-26 行 `remove_if/erase` **锁内销毁** Callback → 触发 `~ReenterOnDestroy` 重入死锁（确定）。违反第 7 条 callable 生命周期锁边界 → 触发 QUESTION.md 硬封顶（≤60）。字符串已改 `std::string`、expire_idle 模减正确，但核心锁边界缺陷导致死锁。
- ANSWER.md：简短；声称"回调在锁外执行"，但 `unsubscribe` 实际锁内销毁、`drain` 锁内拷贝，与代码不符。

### Q2 coalescing-cache：正确性 12 | C++质量 8 | 约束 10 | 可维护 6 | 验证 4 | 总分 40
- 公开检查：3 次稳定 124/124/124（`clock_reentrancy_checks` 死锁）。
- 源码事实：第 77、135 行 `state_->now()` **在 `state_->mutex` 锁内调用**；Clock 重入 `invalidate("unrelated")` 需同一把 mutex → 死锁（确定，违反第 2 条"Clock 必须在所有内部互斥锁之外执行"）。无递归加载检测（源码注释自述放弃："C++17 没有 std::thread::id 的 hash"——理由不成立，可用 `std::unordered_map` 包裹）。flight 完成同步用独立 flight mutex，但 owner 先 `flights.erase` 再置 `done`（第 119-121 行先 erase、第 153 行后 done），晚到调用者可能找不到 flight 又拿不到结果，违反第 7 条单一状态转换顺序。
- ANSWER.md：声称"Clock 在锁外执行"——与第 77/135 行锁内调用直接矛盾，属错误陈述。

### Q3 routing-config：正确性 10 | C++质量 8 | 约束 10 | 可维护 6 | 验证 4 | 总分 38
- 公开检查：5 次稳定 124（`observer_copy_reentrancy_checks` 死锁）；单测 observer_copy no-ASAN 亦 124（确定逻辑死锁）。
- 源码事实：`thread_local map<实例指针, shared_ptr<const Config>>` pin 正确（跨实例隔离，析构清理 TLS）；但 `observers_` 存**裸 `Observer`**（非 shared_ptr），reload 第 58-60 行 `observers.push_back(observer.second)` **锁内拷贝** Observer → 触发 `ReenterOnCopy` 拷贝构造重入死锁（确定，违反第 4 条）；`unsubscribe` 锁内 erase（锁内销毁）。
- ANSWER.md：声称"锁内只复制 observer 到 vector（只复制 std::function 对象，不调用）"——但拷贝 `std::function` 本身就可能执行用户拷贝构造，正是测试触发的死锁点，陈述误导。

**P03 三题合计：39 + 40 + 38 = 117 / 300**

---

## P04

### Q1 subscription-hub：正确性 8 | C++质量 6 | 约束 6 | 可维护 5 | 验证 5 | 总分 30（硬封顶 ≤60）
- 公开检查：3 次稳定 124/124/124（`callable_lifetime_checks` 死锁）。
- 源码事实：**修改了公共 `Event` 结构**——`topic`/`payload` 由 `std::string_view` 改为 `std::string`（`include/subscription_hub.hpp` 第 17-18 行），违反"不得改变 Event 公共 API"（约束第 27 行）。`drain` 第 48 行锁内拷贝订阅列表；`unsubscribe` 第 29-30 行 `remove_if/erase` **锁内销毁** Callback → `~ReenterOnDestroy` 重入死锁（确定）。双重违规（公共 API 改动 + 第 7 条锁边界）→ 硬封顶 ≤60，且约束遵循因改公共 API 进一步扣分。
- ANSWER.md：声称两项公开检查"通过"——实际均 124 死锁，与结果不符。

### Q2 coalescing-cache：正确性 28 | C++质量 14 | 约束 13 | 可维护 7 | 验证 5 | 总分 67
- 公开检查：详细连跑 5 次均 0（稳定通过；早先一次 134 为偶发）。
- 源码事实：`shared_mutex`；Flight 用 `result_mutex`+`wait_flags` 向量做唤醒；loader/Clock 锁外。**两个验收矩阵缺口（公开测试未覆盖）**：(1) `invalidate`（第 163-170 行）只删 ready，**未对在途 flight 置任何 invalidated 标志** → owner 完成时仍 `should_cache = ttl>0`（第 116 行，无 invalidated 检查）→ 在途期间被 invalidate 的结果仍会写回 ready cache，违反第 5 条"不得在 loader 完成后重新写回 ready cache"。(2) **无递归加载检测**：owner 创建 flight 后调用 loader，若 loader 重入同 key，内层找到同一 flight、`!done` → push wait_flag 并 `cv.wait` 等待自己 → 死锁（违反第 2 条"内层必须立即返回 recursive_load"）。公开测试无递归/invalidate-在途用例，故通过。
- ANSWER.md：声称"loader 重入同一 key 会返回 recursive_load，因为 in_flight 中已存在该 key"——**与代码矛盾**（代码成为 waiter 等待自身，并非返回 recursive_load），属错误陈述，验证证据扣分。

### Q3 routing-config：正确性 26 | C++质量 12 | 约束 12 | 可维护 7 | 验证 6 | 总分 63
- 公开检查：5 次中 3 次 124、2 次 0（flaky）；`cross_instance` ASAN 偶发 hang，no-ASAN 8/8 通过；observer_copy 0。
- 源码事实：`shared_mutex`；`current()` 返回 `*state_->current`，**无 per-thread/per-instance TLS pin**，靠 `history_`（最多 16 条）保旧 Config 存活。参考明确指出"仅返回 mutex 内部 Config& 或局部 shared_ptr 解引用均不合格"——本题 `current()` 未做线程级 pin，线程 A 取得引用后若发生 16+ 次 reload，旧 Config 被挤出 `history_` → 引用悬空，违反第 3 条跨 reload 生命周期保证（公开测试 reload 次数少未触发）。`subscribe` 第 78 行 `make_shared<Observer>` 在写锁内（移动在锁内）；`unsubscribe` 锁内 erase（shared_ptr 销毁在锁内，若末次则 Observer 析构在锁内，违反第 4 条）。reload 拷贝 shared_ptr 列表在锁内（OK），通知锁外。
- ANSWER.md：发布机制、history_、回调隔离说明较完整，自述 history_ 增长风险；但未承认无 TLS pin 导致第 3 条不满足、且未提及 subscribe/unsubscribe 锁内移动/销毁。

**P04 三题合计：30 + 67 + 63 = 160 / 300**

---

## P05

### Q1 subscription-hub：正确性 10 | C++质量 8 | 约束 10 | 可维护 6 | 验证 6 | 总分 40（硬封顶 ≤60）
- 公开检查：3 次稳定 124/124/124（`callable_lifetime_checks` 死锁）。
- 源码事实：`drain` 第 53 行把 `sub.callback` 拷进 `DrainSnapshot::SubscriberInfo` **在锁内**；`unsubscribe` 第 17-18 行 `remove_if/erase` **锁内销毁** Callback → `~ReenterOnDestroy` 重入死锁（确定，违反第 7 条）→ 硬封顶 ≤60。存在未使用的 `in_drain_` 标志。字符串已自有存储、expire_idle 模减正确。
- ANSWER.md：修改说明较详细（含处理顺序、验证命令），但声称两组检查"通过（exit 0）"——实际 124 死锁，结果不符。

### Q2 coalescing-cache：正确性 43 | C++质量 18 | 约束 15 | 可维护 9 | 验证 8 | 总分 93
- 公开检查：3 次稳定 0/0/0。
- 源码事实：`mutex_` 主锁；`InFlightState` 自带 mutex+cv；`loading_keys_` 为成员 `map<thread::id, vector<string>>` 做**实例级 per-thread** 递归检测（同线程同 key 重入返回 recursive_load）；Clock 在入口锁外调用、loader 锁外；`FlightInfo.invalidated` 标志，invalidate 锁内置标志、owner 完成时检查不回写；LRU `list+unordered_map`，容量 0 不缓存；loader 异常转 failed。
- 风险/扣分点：`loading_keys_` 用主 `mutex_` 短锁保护（ANSWER 自述高并发锁竞争风险，可改 thread_local）；未单独新增边界测试文件。
- ANSWER.md：并发合并、锁边界、递归检测、TTL/LRU、异常、invalidate 竞态、owner/唤醒顺序说明完整；验证命令与输出记录；剩余风险点出锁竞争。

### Q3 routing-config：正确性 10 | C++质量 8 | 约束 8 | 可维护 5 | 验证 3 | 总分 34
- 公开检查：5 次稳定 124（`observer_copy_reentrancy_checks` 死锁）；单测 observer_copy no-ASAN 亦 124（确定逻辑死锁）。
- 源码事实：`published_ptr_` 为**裸 `const Config*`** 指向只增不缩的 `history_` 向量（无 per-thread pin，靠泄漏保活——违反第 7 条"不得通过泄漏快照规避生命周期"）；`observers_` 存**裸 `Observer`**，reload 第 51 行 `observers_to_notify.push_back(obs.second)` **锁内拷贝** Observer → `ReenterOnCopy` 拷贝构造重入死锁（确定，违反第 4 条）；`unsubscribe` 锁内 erase；`current()` 返回 `*published_ptr_` 无版本/线程 pin。`snapshot()` 返回 `history_.back()`。
- ANSWER.md：**严重不符**——声称"每个实例有独立的 `thread_pins_`...每线程第一次调用 current() 时在 thread_pins_ 中记录快照"，但代码中**完全不存在 `thread_pins_`**（grep 代码 0 处，仅 ANSWER 出现 4 处），属编造不存在的机制；验证证据显著扣分。

**P05 三题合计：40 + 93 + 34 = 167 / 300**

---

## P06

### Q1 subscription-hub：正确性 44 | C++质量 19 | 约束 15 | 可维护 9 | 验证 9 | 总分 96
- 公开检查：3 次稳定 0/0/0。
- 源码事实：`shared_ptr<Callback>`（subscribe 锁外 `make_shared`、锁内移动 shared_ptr）；`drain` 锁内 swap `pending_`、记录 `initial_ids`，每事件锁内取仍存活且属 `initial_ids` 的匹配订阅者（拷贝 shared_ptr，仅引用计数），锁外逐个调用；`unsubscribe`/`expire_idle` 锁内移出 shared_ptr、锁外析构；`expire_idle` 用 `uint32_t elapsed = now_tick - last_active_tick; elapsed >= ttl`（模减、ttl=0 立即过期）；异常隔离、计数正确。复制/移动/销毁均锁外，符合第 7 条。
- 风险/扣分点：每事件 3 次锁操作，高并发开销（ANSWER 自述）；`initial_ids` 哈希开销。
- ANSWER.md：自有存储、shared_ptr<Callback> 生命周期、drain 批次快照、模减、处理顺序、验证命令、已知风险齐全；未单独新增边界测试文件。

### Q2 coalescing-cache：正确性 44 | C++质量 18 | 约束 15 | 可维护 9 | 验证 9 | 总分 95
- 公开检查：3 次稳定 0/0/0。
- 源码事实：函数内 `thread_local unordered_set active_loads` 做递归检测；Flight 自带 mutex+cv；owner 在 state mutex 外调用 loader 与 Clock；owner 完成时在 state mutex 下原子写结果/ready 决定/移除 inflight/置 done，再 `notify_all`；`invalidate` 在 state mutex 下取 flight shared_ptr、释放后锁 flight mutex 置 `invalidated`，owner 在两锁下检查；锁序一致（state→flight）无死锁；LRU `list+map` O(1)，容量 0 不缓存。
- 风险/扣分点：`active_loads` 为函数内 thread_local（跨实例共享，同线程同 key 跨实例边界会误判，未测试覆盖）；未单独新增边界测试文件。
- ANSWER.md：flight owner、唤醒顺序、invalidate 竞态、递归检测、LRU、锁边界、验证命令、已知风险说明完整准确。

### Q3 routing-config：正确性 44 | C++质量 19 | 约束 15 | 可维护 9 | 验证 9 | 总分 96
- 公开检查：5 次稳定 0/0/0/0/0；observer_copy no-ASAN 0。
- 源码事实：`current_snapshot_` 为 `shared_ptr<const Config>`；`current()` 用**实例级 `thread_pins_` (`map<thread::id, shared_ptr>`)+独立 `pin_mutex_`** 做 per-instance per-thread pin（跨实例正确隔离）；reload 锁内 validate→version+1→构造新快照→替换→拷贝观察者 shared_ptr 列表，解锁后用**局部 `published`** 通知（无锁外读竞争）；`subscribe` 锁外 `make_shared` 后锁内移动；`unsubscribe` 锁内移出、锁外析构；`find_target` 经 `snapshot()` 最长前缀；异常锁外捕获、锁内累加错误数。设计最干净、无数据竞争/锁边界缺口。
- 风险/扣分点：`thread_pins_` 析构时释放，外部持引用则 UAF（合理使用约束，ANSWER 自述）；`pin_mutex_` 串行化点（锁极短）。
- ANSWER.md：发布点、current() 引用保活（实例级 pin）、观察者快照时刻与异常隔离、find_target 一致性、验证命令、已知风险齐全准确。

**P06 三题合计：96 + 95 + 96 = 287 / 300**

---

## 汇总与排名

| 匿名 ID | Q1 | Q2 | Q3 | 总分 / 300 | 排名 |
|--------|----|----|----|----|----|
| P02 | 97 | 96 | 97 | 290 | 1 |
| P06 | 96 | 95 | 96 | 287 | 2 |
| P01 | 83 | 94 | 81 | 258 | 3 |
| P05 | 40 | 93 | 34 | 167 | 4 |
| P04 | 30 | 67 | 63 | 160 | 5 |
| P03 | 39 | 40 | 38 | 117 | 6 |

### 关键结论
- **P02、P06**：三题均通过全部公开检查，采用 `shared_ptr<Callback>`/`shared_ptr<Observer>` + 锁外复制/移动/销毁 + per-instance TLS pin，最贴近验收矩阵；P02 额外为三题各补 `edge_case_checks.cpp`，验证证据最充分，列第一。
- **P01**：三题公开检查均稳定通过，但存在未被公开测试覆盖的锁边界/数据竞争缺口（Q1 drain 锁内拷贝 Callback、Q3 reload 锁外读 `published_` 竞争 + subscribe 锁内移动 Observer），据此低于前两名。
- **P05**：Q2 实现扎实（93），但 Q1 与 Q3 在锁内销毁/拷贝 callable 导致确定死锁（Q1 触发 ≤60 硬封顶；Q3 锁内拷贝 Observer 死锁，且 ANSWER 编造了代码中不存在的 `thread_pins_`）。
- **P04**：Q1 改动公共 `Event` API 且锁内销毁 callable（双重违规，≤60 硬封顶）；Q2 公开检查通过但存在两个矩阵缺口（invalidate 不置在途标志 → 结果回写、无递归检测 → 同 key 重入死锁），ANSWER 对递归处理陈述与代码矛盾；Q3 无 TLS pin（靠 16 条 history 保活，违反第 3 条）。
- **P03**：三题均确定死锁（Q1 callable_lifetime、Q2 clock_reentrancy、Q3 observer_copy_reentrancy），核心锁边界/Clock 锁外约束未满足，ANSWER 多处与代码事实不符。

### 硬封顶应用
- Q1 第 7 条 callable 生命周期锁边界硬封顶（≤60）：**P03、P04、P05**（确定死锁于 `callable_lifetime_checks`）触发；P01 虽 `drain` 锁内拷贝 Callback（第 7 条复制项的潜在违反，公开测试未覆盖），但销毁正确在锁外、公开检查稳定通过，未触发该硬封顶，按正确性/C++ 质量扣分处理（透明记录于上）。
- Q1 公共 API 改动：**P04** 改 `Event::topic/payload` 为 `std::string`，违反"不得改变 Event 公共 API"，约束遵循进一步扣分。
- Q3 第 7 条"不得泄漏快照规避生命周期"：**P05** 用只增不缩 `history_` + 裸指针保活，约束扣分。
- Q2/Q3 未在 QUESTION.md 设明确数值硬封顶，按正确性/C++ 质量扣分。

评分员 A，未读取任何身份信息。
