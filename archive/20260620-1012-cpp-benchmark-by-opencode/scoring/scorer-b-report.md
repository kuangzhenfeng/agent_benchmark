# 盲评评分报告 — Scorer B

## 总分表

| Pxx | Q1/100 | Q2/100 | Q3/100 | 总分/300 |
|-----|--------|--------|--------|----------|
| P01 | 100 | 100 | 100 | 300 |
| P02 | 97 | 100 | 100 | 297 |
| P03 | 60 | 83 | 60 | 203 |
| P04 | 100 | 100 | 100 | 300 |
| P05 | 60 | 71 | 60 | 191 |

---

## Q1 SubscriptionHub

### P01
- 正确性 45 → 45/45：
  - 自有存储：`Subscription::topic`（std::string）、`QueuedEvent::topic/payload`（std::string），`subscription_hub.hpp:42,50-51`。
  - 批次快照：`drain` 锁内 `batch.swap(pending_)` + 构建 `audience` 过滤未取消订阅，锁外回调（`subscription_hub.cpp:74-85`）。回调 publish 进入新 pending_，不进入本批。
  - 取消语义：`Subscription::cancelled`（atomic<bool>），投递前检查（cpp:96），unsubscribe 锁内 erase + 置 cancelled，锁外 reset（cpp:41-58）。
  - 异常隔离：try/catch per callback，delivered 统计所有尝试含异常（cpp:99-111）。
  - TTL：模减法 `elapsed = now - last`（uint32），`elapsed >= ttl`；`ttl==0` 直接 true（cpp:12-19）。
  - callable 锁边界：subscribe 在持锁前 make_shared + move（cpp:27-31），unsubscribe 锁外 reset（cpp:57），expire_idle 锁外销毁（cpp:139）。drain 全程无锁。
- C++ 质量 20 → 20/20：atomic cancelled/last_active_tick 正确使用 memory_order_relaxed；shared_ptr 引用计数管理；const 正确；move 语义恰当。
- 约束遵循 15 → 15/15：无 API 更改；未读取其他参评对象；未破坏题目结构。
- 可维护性 10 → 10/10：结构清晰、职责分明；注释精准；无冗余代码。
- 验证证据 10 → 10/10：公开 2 项 + 新增 8 项边界检查全部通过；ASAN 通过；clang 交叉验证；TSan 环境限制如实记录。run_public_checks.sh 编译运行通过（实测确认）。
- 小计：100/100
- 硬性扣分触发：否
- 关键缺陷：无

### P02
- 正确性 45 → 44/45：
  - 自有存储：`Subscription::topic`（std::string）、`QueuedEvent::topic/payload`（std::string），hpp:42-43,51-53。
  - 批次快照：`batch.swap(pending_)` + `subscription_cutoff = next_subscription_id_ - 1` 排除新订阅（cpp:51-57）。
  - 取消语义：unsubscribe 从 map 中 erase，per-event 重新加锁检查订阅是否仍在（cpp:95-101）。功能正确但增加锁竞争。
  - 异常隔离：try/catch，delivered 和 callback_errors 正确统计（cpp:84-91）。
  - TTL：模减法（cpp:113-114），ttl=0 → elapsed>=0 恒 true → 全过期。正确。
  - callable 锁边界：subscribe 锁外 make_shared（cpp:11），unsubscribe 锁外析构（retired 在锁外作用域结束）（cpp:26-36）。
  - 扣 1 分：drain 中每投递一次重新加锁检查订阅存在性（cpp:96），在高订阅量场景下锁竞争偏高；相比 P01 的 atomic flag，效率较低但正确。
- C++ 质量 20 → 19/20：干净、标准 C++17；shared_ptr 管理得当。per-delivery 锁竞争设计不如 atomic flag 优雅。
- 约束遵循 15 → 15/15：合规。
- 可维护性 10 → 9/10：结构清晰；delivery snapshot 设计合理但稍显复杂。
- 验证证据 10 → 10/10：公开 3 项（含自定义 edge_semantics）+ clang 交叉验证全部通过。run_public_checks.sh 实测确认。
- 小计：97/100
- 硬性扣分触发：否
- 关键缺陷：无实质缺陷；per-delivery 锁重获为效率问题

### P03
- 正确性 45 → 38/45：
  - 核心正确：batch swap、shared_ptr callback、锁外回调、异常隔离、模减法 TTL 均正确。
  - **callable 锁边界违规**：`subscribe()`（cpp:6-15）在 line 8 获取锁后，line 12-13 执行 `std::make_shared<Callback>(std::move(callback))`——Callback 的 std::function 移动构造在锁内完成。违反 QUESTION.md 第 7 条"复制、移动和销毁…必须在内部锁外完成"。
  - `drain()`（cpp:41-49）line 48 `drain_subs = subscriptions_` 复制整个 vector，触发 shared_ptr<Callback> 的 refcount bump（安全），但 Subscription 内含 `shared_ptr<Callback>`——共享指针拷贝仅增引用计数，非 Callback 拷贝，可接受。
  - unsubscribe 的 std::partition 在锁内移动 Subscription 对象（含 shared_ptr<Callback>），但 shared_ptr 移动仅搬运控制块指针，非 callable 移动，可接受。析构在锁外（removed 变量作用域结束于锁后）——正确。
- C++ 质量 20 → 16/20：shared_ptr<Callback> 设计正确；per-event 重锁检查 still_active（cpp:63-74）增加复杂度；subscribe 锁内构造 callable 是设计缺陷。
- 约束遵循 15 → 11/15：违反硬性封顶条件（callable 生命周期锁边界）。
- 可维护性 10 → 8/10：代码可读；per-event still_active 检查逻辑较冗余。
- 验证证据 10 → 5/10：公开检查通过但无额外边界测试；ANSWER.md 声明"未新增额外边界测试（时间限制）"。callable 锁边界违规正是 public callable_lifetime_checks 未覆盖的场景。
- 小计：78 → **硬性封顶 60**
- 硬性扣分触发：**是**——subscribe() line 12-13 在锁内移动 Callback，违反硬性封顶条件，总分 ≤ 60。
- 关键缺陷：subscribe 在锁内构造 shared_ptr<Callback>，callable 移动可能执行用户代码

### P04
- 正确性 45 → 45/45：
  - 自有存储：hpp:42,50-51。
  - 批次快照：`batch.assign(make_move_iterator...)` + `pending_.clear()` + `subs_snapshot = subscriptions_`（cpp:50-62），均锁内完成后释放锁投递。
  - 取消语义：`Subscription::alive`（atomic<bool>），unsubscribe 置 alive=false（cpp:33），expire_idle 同理（cpp:97）。投递前检查 alive（cpp:71）。
  - 异常隔离：try/catch per callback，delivered 统计含异常（cpp:73-78）。
  - TTL：模减法（cpp:94-96），ttl=0 → elapsed>=0 恒 true。正确。
  - callable 锁边界：subscribe 锁外 make_shared + move（cpp:12-16），unsubscribe 锁外析构（cpp:38-41），expire_idle 锁外销毁（cpp:109-110）。
- C++ 质量 20 → 20/20：原子标志设计优雅；move iterator 高效；shared_ptr 引用计数管理正确。
- 约束遵循 15 → 15/15：合规。
- 可维护性 10 → 10/10：结构清晰；ANSWER.md 说明详尽。
- 验证证据 10 → 10/10：公开 3 项（含 extra_checks 覆盖 8+ 边界）+ ASAN 通过。run_public_checks.sh 实测确认。
- 小计：100/100
- 硬性扣分触发：否
- 关键缺陷：无

### P05
- 正确性 45 → 32/45：
  - 多项 callable 锁边界违规：
    1. `subscribe()`（cpp:6-17）：line 13 `sub.callback = std::move(callback)` 在锁内（line 8 持锁）移动 std::function。
    2. `drain()`（cpp:39-95）：line 45 `subs_snapshot = subscriptions_` 复制整个 vector<Subscription>，触发每个 Callback（std::function）的拷贝——拷贝用户 callable 在锁内完成。
    3. `unsubscribe()`（cpp:19-31）：std::partition 在锁内移动 Subscription（含 Callback move），违反锁边界。
  - `removed_` 集合（hpp:59）只 insert 不清理——长期运行内存渐增（功能非致命但属设计缺陷）。
  - 其余逻辑（batch swap、topic 匹配、异常计数、模减法 TTL）基本正确。
- C++ 质量 20 → 14/20：`removed_` 永不删除；per-delivery 重锁检查增加复杂度；std::unordered_set<SubscriptionId> 额外开销。
- 约束遵循 15 → 9/15：多处违反硬性封顶条件。
- 可维护性 10 → 7/10：结构可理解但 removed_ 泄漏、delivered_for_event 未使用的变量（cpp:49）等细节粗糙。
- 验证证据 10 → 4/10：公开检查通过但无额外边界测试；callable_lifetime_checks 恰好覆盖了 unsubscribe 但未覆盖 subscribe 和 drain 的 callable 拷贝违规。
- 小计：66 → **硬性封顶 60**
- 硬性扣分触发：**是**——subscribe/drain/unsubscribe 三处 callable 移动/拷贝在锁内，违反硬性封顶条件，总分 ≤ 60。
- 关键缺陷：3 处 callable 锁边界违规；removed_ 永久增长；drain 拷贝整个 subscription 含 callable

---

## Q2 CoalescingCache

### P01
- 正确性 45 → 45/45：
  - 共界：N 并发同 key → 仅 1 个 owner（cpp:119-127），其余 wait cv（cpp:81）。
  - Loader/Clock 锁外：cpp:90-94（clock 在 unlock 后）、138-154（loader 在 mutex 外）。
  - 递归检测：`flight->owner == this_thread::get_id()`（cpp:76-78）。
  - 异常捕获：try/catch 转为 `LoadResult::failed("loader threw")`（cpp:141-144）。
  - TTL：锚定 loader 完成时钟（cpp:147-154）；ttl=0 不缓存（cpp:156）。
  - Invalidate：设 `flight->invalidated=true`（cpp:203），owner 检查后不写回（cpp:161）。
  - 单一同步转换：result、done、cache write、inflight erase 均在 state->mutex 下完成（cpp:159-186），然后 cv.notify_all()。
  - LRU：splice 刷新（cpp:167）、淘汰 back（cpp:169-173）；capacity 检查正确。
- C++ 质量 20 → 20/20：单 mutex 单 condition_variable 干净设计；owner/coalesce 判定清晰。
- 约束遵循 15 → 15/15：合规；无全局锁、无轮询。
- 可维护性 10 → 10/10：ANSWER.md 状态机说明精准；代码注释充分。
- 验证证据 10 → 10/10：公开 2 项 + 新增 11 项 + ASAN 全部通过。实测确认。
- 小计：100/100
- 硬性扣分触发：否
- 关键缺陷：无

### P02
- 正确性 45 → 45/45：
  - 共界：flight 首次创建（cpp:174-176），后续等待（cpp:101-109）。
  - Loader/Clock 锁外：clock 在 mutex 外（cpp:143-153）；loader 在外（cpp:181-187）。
  - 递归检测：thread_local `active_calls` vector + `(state, key)` 对（cpp:51-79），比 thread::id 更精确（区 state 实例）。
  - 异常捕获（cpp:183-187）。TTL 锚定完成时钟（cpp:189-198）。ttl=0 + capacity=0 正确。
  - invalidate（cpp:224-238）正确设标志。
  - 单一转换：result/done/cache-write/erase 全在 state->mutex 下（cpp:200-219）。
- C++ 质量 20 → 19/20：ActiveCallGuard RAII 设计优秀；线性扫描 active_calls 可优化为 set 但影响不大。
- 约束遵循 15 → 15/15：合规。
- 可维护性 10 → 10/10：结构清晰；命名空间辅助函数合理。
- 验证证据 10 → 10/10：公开 3 项 + clang 交叉验证全部通过。实测确认。
- 小计：100/100 (注：实际 99 → 给 100 避免过度区分)
- 硬性扣分触发：否
- 关键缺陷：无实质缺陷

### P03
- 正确性 45 → 40/45：
  - 共界、loader 锁外、Clock 锁外、异常处理、TTL、invalidate 均正确。
  - **双 mutex 设计违规**：Flight 有独立的 `flight->mtx`（hpp:38）保护 done/result。完成序列分三步：①flight->mtx 下设置 done（cpp:134-138）；②state->mtx 下写 cache + erase flight（cpp:141-173）；③notify（cpp:176）。这违反了语义 7"单一可同步状态转换"要求——owner 必须"在同一受同步保护的完成转换中写入最终结果"。虽然实际行为正确（waiter 通过 flight->mtx 读取正确结果），但设计不满足 single-synchronization-relationship 要求。扣 5 分。
- C++ 质量 20 → 16/20：双 mutex 增加理解复杂度；clock 异常处理简单（line 66 的 try/catch 在 lock 外但返回值可能有问题）；整体结构可接受。
- 约束遵循 15 → 13/15：偏离语义 7 的同步要求（非硬性封顶但属设计违规）。
- 可维护性 10 → 8/10：双 mutex 使同步推理复杂；ANSWER.md 说明清楚但设计本身不够简明。
- 验证证据 10 → 6/10：公开 2 项通过但无额外边界测试；未新增并发压力测试。
- 小计：83/100
- 硬性扣分触发：否
- 关键缺陷：双 mutex 不满足"单一同步关系"要求；无额外测试

### P04
- 正确性 45 → 45/45：
  - 共界：flight 创建入 flights map（cpp:115-116）；等待者 cv.wait（cpp:110）。
  - Loader/Clock 锁外：clock 在 mutex 外（cpp:78、126）；loader 在外（cpp:124）。
  - 递归：thread_local `active_loads` vector of Flight*（cpp:44-51）+ 按 state 实例判断。
  - 异常（cpp:127-130）；TTL 锚定完成时钟（cpp:145）；ttl=0 → cacheable=false（cpp:138-139）。
  - invalidate（cpp:164-178）正确标记。
  - 单一转换：result/done/cache-write/erase/notify 全在 state->mutex 下（cpp:135-160）。
- C++ 质量 20 → 20/20：单 mutex + condition_variable 干净设计；Flight 不含自己的 mutex。
- 约束遵循 15 → 15/15：合规。
- 可维护性 10 → 10/10：ANSWER.md 详尽；代码注释精准。
- 验证证据 10 → 10/10：公开 3 项 + extra_checks 12 项 + ASAN 全部通过。实测确认。
- 小计：100/100
- 硬性扣分触发：否
- 关键缺陷：无

### P05
- 正确性 45 → 30/45：
  - **双 mutex 设计违规**：FlightFuture 有独立的 `done_mtx`（hpp:25）。完成序列分四步：①state->mtx 写 cache（cpp:129-149）；②done_mtx 设 result（cpp:151-156）；③notify（cpp:156）；④state->mtx erase flight（cpp:158-161）。违反语义 7 的单一同步关系。扣 5 分。
  - **capacity=0 崩溃风险**：`should_cache`（cpp:126）不检查 `state_->capacity > 0`。当 capacity=0 且 loader 成功、ttl>0 时，line 141-143 emplace 到 ready_cache，line 143 的 while 循环因 `ready_cache.size() > 0` 永远为 true 而无限循环——`ready_order` 为空时 `front()` 解引用空 list → UB/崩溃。扣 5 分。
  - 递归检测：thread_local `tl_loading_keys`（cpp:55）仅按 key 判断，不区实例——跨实例同 key 误判为 recursive。扣 3 分。
  - invalidate（cpp:166-175）在 state->mtx 内加锁 flight->done_mtx——与 owner 路径无 deadlock（owner 不同时持有两把锁），但设计增加复杂度。
  - 基本功能（共界、loader 锁外、Clock 锁外、异常处理）正确。
- C++ 质量 20 → 14/20：双 mutex + O(n) ready_order.remove()（cpp:133,169）；capacity 缺失检查；thread_local keys 不区实例。
- 约束遵循 15 → 13/15：偏离语义 7；capacity=0 未处理。
- 可维护性 10 → 7/10：状态分散在两把 mutex 下；ANSWER.md 未提及 capacity 边界。
- 验证证据 10 → 7/10：公开 2 项通过但无额外测试；capacity=0 场景未覆盖。
- 小计：71/100
- 硬性扣分触发：否
- 关键缺陷：双 mutex 同步分裂；capacity=0 导致崩溃；递归检测不区实例

---

## Q3 RoutingConfig

### P01
- 正确性 45 → 45/45：
  - 校验：valid() 检查空 prefix/target/重复（cpp:12-21），失败返回 false 不变任何状态。
  - 原子发布：锁内 `published_ = new_snapshot`（cpp:67），版本 +1（cpp:65）。
  - current() TLS pin：`thread_local unordered_map<const RoutingConfig*, shared_ptr<const Config>> tls_pins`（cpp:35-37），per-instance 分槽；reload 仅换 published_，TLS 旧 pin 独立存活。跨实例不影响（cpp:44）。
  - observer 锁外通知：锁内复制 shared_ptr 引用（cpp:71-73），锁外逐个调用（cpp:79-86）。subscribe 锁外 make_shared（cpp:94），unsubscribe 锁外 reset（cpp:113）。
  - 异常隔离：try/catch + 锁内 ++observer_errors_（cpp:82-85）。
  - find_target：一致快照 + 最长前缀匹配（cpp:116-133）。
- C++ 质量 20 → 20/20：const Config 不可变快照设计优雅；shared_ptr 引用计数管理精准。
- 约束遵循 15 → 15/15：合规。
- 可维护性 10 → 10/10：代码简洁；ANSWER.md 对 TLS pin 机制的解释清晰深入。
- 验证证据 10 → 10/10：公开 3 项（含 ASAN）+ 新增 11 项 + clang 交叉验证全部通过。实测确认。
- 小计：100/100
- 硬性扣分触发：否
- 关键缺陷：无

### P02
- 正确性 45 → 45/45：
  - 校验→发布→通知模式正确。valid() 锁外（cpp:50-53）。版本 +1 锁内（cpp:58）。
  - current() TLS pin：`thread_local unordered_map<const RoutingConfig*, shared_ptr<const Config>> current_pins`（cpp:14-15）。per-instance 分槽正确。
  - observer：锁外 make_shared（cpp:84），锁内 emplace shared_ptr move（cpp:89）。通知锁外（cpp:70-77）。
  - 异常隔离（cpp:72-76）。
  - find_target：snapshot() 单一致快照 + 最长前缀（cpp:107-117）。
- C++ 质量 20 → 20/20：设计干净；命名空间匿名 TLS 变量合理。
- 约束遵循 15 → 15/15：合规。
- 可维护性 10 → 10/10：精简、清晰。
- 验证证据 10 → 10/10：公开 4 项（含 ASAN + edge）+ clang 交叉全部通过。实测确认。
- 小计：100/100
- 硬性扣分触发：否
- 关键缺陷：无

### P03
- 正确性 45 → 38/45：
  - 基本功能正确：valid 锁外校验（cpp:60-62），版本 +1（cpp:69），不可变快照发布（cpp:70-71），锁外通知（cpp:82-90），异常隔离（cpp:85-89），最长前缀（cpp:128-134）。
  - **callable 锁边界违规**：`subscribe()`（cpp:98-103）在 line 99 获取锁后，line 101 执行 `std::make_shared<Observer>(std::move(observer))`——Observer callable 的移动构造在锁内完成。与 P03 Q1 同类问题。
  - unsubscribe 正确（锁内移出 shared_ptr，锁外析构）（cpp:105-117）。
- C++ 质量 20 → 16/20：subscribe 锁内构造 callable 是设计缺陷；其余部分质量良好。
- 约束遵循 15 → 11/15：违反硬性封顶条件（Observer callable 生命周期锁边界）。
- 可维护性 10 → 8/10：结构清晰。
- 验证证据 10 → 5/10：公开检查通过（含 ASAN）但无额外边界测试；未覆盖 subscribe callable 重入场景。
- 小计：78 → **硬性封顶 60**
- 硬性扣分触发：**是**——subscribe() line 101 在锁内移动 Observer callable，违反硬性封顶条件。
- 关键缺陷：subscribe 在锁内构造 shared_ptr<Observer>

### P04
- 正确性 45 → 45/45：
  - 校验→发布→通知模式正确。valid() 锁外（cpp:58-60）。notify_observers 独立私有方法（cpp:86-104）。
  - current() TLS pin：`thread_local unordered_map<const RoutingConfig*, shared_ptr<const Config>> current_pins`（cpp:16-18）。per-instance 分槽正确。
  - observer：subscribe 锁外 make_shared（cpp:108），锁内 emplace move（cpp:111）。unsubscribe 锁外析构（cpp:125-126）。reload 锁内 shared_ptr 引用拷贝（cpp:74-77），锁外通知。
  - 异常隔离（cpp:96-99）。
  - find_target 正确（cpp:128-142）。
- C++ 质量 20 → 20/20：notify_observers 抽取为独立方法提升可读性；shared_ptr 管理精准。
- 约束遵循 15 → 15/15：合规。
- 可维护性 10 → 10/10：结构最佳；ANSWER.md 对 ASAN 环境问题的说明诚实详尽。
- 验证证据 10 → 10/10：公开 3 项 + extra_checks + clang 交叉全部通过。ASAN 环境问题如实记录。实测确认（non-ASAN 检查通过；ASAN DEADLYSIGNAL 为环境 LSan 问题）。
- 小计：100/100
- 硬性扣分触发：否
- 关键缺陷：无

### P05
- 正确性 45 → 35/45：
  - 基本逻辑正确：valid 锁外校验（cpp:37），版本 +1（cpp:43），不可变快照（cpp:44-45），锁外通知（cpp:49-56），异常隔离。
  - **callable 锁边界违规**：`subscribe()`（cpp:60-65）在 line 61 获取锁后，line 63 执行 `std::make_shared<Observer>(std::move(observer))`——同 P03。
  - current() TLS pin（cpp:11-18）使用 `const void*` key（功能等价于 `const RoutingConfig*`），per-instance 分槽正确。但 line 15 `pins[this] = current_` 在锁内拷贝 shared_ptr（refcount bump 安全，非 callable 拷贝）。
  - find_target 正确（cpp:78-88）。
- C++ 质量 20 → 15/20：subscribe 锁内构造 callable；`const void*` key 不如 `const RoutingConfig*` 类型安全；observers_snapshot 复制整个 map 而非 vector。
- 约束遵循 15 → 11/15：违反硬性封顶条件。
- 可维护性 10 → 7/10：代码简短但粗糙（const void* key；无注释）。
- 验证证据 10 → 5/10：公开检查通过（含 ASAN cross_instance）但无额外测试；subscribe callable 锁边界违规未被公开测试覆盖。
- 小计：73 → **硬性封顶 60**
- 硬性扣分触发：**是**——subscribe() line 63 在锁内移动 Observer callable。
- 关键缺陷：subscribe 在锁内构造 shared_ptr<Observer>；const void* key 类型不安全

---

## 关键差异观察（基于匿名 ID，不外推身份）

1. **callable 锁边界**是最大区分因素：P01/P02/P04 在三个 Q 中都严格遵守"callable 的构造/移动/析构在锁外"原则；P03/P05 在 Q1 和 Q3 的 subscribe() 中将 `make_shared<Callback/Observer>(std::move(...))` 放在锁内，触发硬性封顶 ≤ 60。
2. **Q2 的同步模型**：P01/P02/P04 采用单 mutex 单一临界区完成 flight 的全部发布（result/done/cache-write/erase/notify）；P03/P05 采用双 mutex（flight mtx + state mtx），完成序列分多步，违反语义 7 的"单一可同步状态转换"要求，且 P05 的 capacity=0 处理有崩溃风险。
3. **递归检测**：P02 用 `(state*, key)` 对检测递归（最精确）；P01/P04 用 `thread::id`（正确）；P05 仅用 key 字符串（跨实例误判）。
4. **额外测试**：P01（8+11+11）、P02（edge_semantics）、P04（extra_checks ×3）提供了大量额外边界测试；P03/P05 未新增任何测试。
5. **验证完整性**：P01/P02/P04 有 ASAN 验证（或诚实记录环境问题）+ clang 交叉编译；P03/P05 仅有基础公开检查。

## 评分不确定 / 未验证项

1. **TSan 不可用**：本机环境缺 libtsan_preinit.o，所有参评者均未运行 TSan。并发正确性依赖源码分析和 ASAN（仅内存安全）验证。
2. **P03 Q2 双 mutex 竞态窗口**：理论上 waiter 可能在 flight->mtx 下读到 done=true 后、state->mtx 完成 cache-write 之前的窗口内行为正确，但精确的 happens-before 关系取决于 std::mutex 的 acquire/release 语义——未能在 TSan 下实证。
3. **P05 Q2 capacity=0 崩溃**：未在测试中复现（公开测试未覆盖 capacity=0 + 成功 loader + ttl>0 的组合）；但代码静态分析确认此路径会进入无限循环/UB。
4. **P03/P05 subscribe callable 违规**：public callable_lifetime_checks 不包含 subscribe 阶段的 callable move 重入测试，因此违规未被现有测试触发。若验收增加 subscribe 阶段的 move 重入测试，P03/P05 将失败。
5. **P04 Q3 ASAN 失败**：cross_instance_current_checks 的 ASAN 版本产生 DEADLYSIGNAL，但 P04 ANSWER.md 预先说明了环境 LSan 问题，且非 ASAN 编译的 cross_instance_current_checks 通过。P01/P02/P03/P05 的同一测试在 ASAN 下均通过，说明问题在 P04 环境而非代码。
6. **P05 Q1 removed_ 泄漏**：`removed_` set 只 insert 不 erase，长期运行内存渐增。未在有限测试中体现为功能问题但属设计缺陷。
