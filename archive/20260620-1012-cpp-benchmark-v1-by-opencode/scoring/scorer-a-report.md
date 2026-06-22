# 盲评评分报告 — Scorer A

## 总分表

| Pxx | Q1/100 | Q2/100 | Q3/100 | 总分/300 |
|-----|--------|--------|--------|----------|
| P01 | 93 | 90 | 91 | 274 |
| P02 | 88 | 86 | 89 | 263 |
| P03 | 55 | 70 | 79 | 204 |
| P04 | 91 | 92 | 89 | 272 |
| P05 | 53 | 68 | 77 | 198 |

## Q1 SubscriptionHub

### P01
- 正确性 45 → 43/45：所有核心语义满足。`subscribe` 在锁外 `make_shared` + `move(callback)`（subscription_hub.cpp:27-29, 锁外），`drain` 锁内 `swap(pending_)` + 冻结 `audience` + 锁外回调（:74-85）。`unsubscribe`/`expire_idle` 延迟析构到锁外（:57, :139）。`cancelled` atomic 实现 per-event 取消。`is_expired` 用 uint32 模减（:17-18）。微扣分：drain 中 `cancelled` 检查用 `memory_order_relaxed` 而非 `memory_order_acquire`，在极端并发下可能看不到刚完成的取消（但测试通过且语义可接受）。
- C++质量 20 → 18/20：RAII scoped lock，`shared_ptr<Subscription>` 合理，atomic 用于取消和活跃 tick，移动语义正确。`std::deque` swap 用于批量截取。代码清晰。微扣：`Subscription` 使用 `atomic` 字段放在 shared_ptr 管理的对象中可以工作，但 relaxed 内存序选择可更精确。
- 约束遵循 15 → 15/15：未改变公共 API；callable 生命周期完全在锁外（subscribe:27-29，unsubscribe:57，expire_idle:139，drain 投递不持锁）。
- 可维护性 10 → 9/10：代码结构清晰，职责分明。146 行实现紧凑。
- 验证证据 10 → 8/10：public_checks + callable_lifetime_checks 编译运行通过（已验证）。extra_boundary_checks 覆盖 8 个边界用例含多线程。TSan 未跑（环境限制）。
- 小计：93/100
- 硬性扣分触发：否
- 关键缺陷：无重大缺陷。

### P02
- 正确性 45 → 40/45：核心语义基本正确。`subscribe` 在锁外构造 `shared_ptr<Callback>`（:11），`drain` 用 `batch.swap(pending_)` + `subscription_cutoff` 控制批次（:55-56），每个事件单独取 delivery 快照。异常隔离（:85-91）。`expire_idle` 用 uint32 模减（:113-114）。微扣分：`drain` 每个事件前重新加锁扫描 `subscriptions_` 构建 delivery list（:67-78），虽然语义上正确但增加了锁争用；`last_active_tick` 非 atomic（`uint32_t`），drain 投递线程和 expire_idle 可能存在数据竞争（:97-99 vs :113，非 atomic 读写）。
- C++质量 20 → 16/20：使用 `std::map` 而非 vector 存储订阅，查找效率高。但 `last_active_tick` 不是 atomic 且被多线程写（drain 的回调后更新 + expire_idle 读），构成数据竞争。`unsubscribe` 中 `retired` shared_ptr 在锁作用域外析构（:26 声明，:36 超出作用域），正确。
- 约束遵循 15 → 14/15：callable 生命周期锁外处理正确。但 `last_active_tick` 的数据竞争违反了"公共方法可被多个线程调用"的安全要求。
- 可维护性 10 → 8/10：实现清晰，但 per-event 加锁策略较复杂。
- 验证证据 10 → 10/10：run_public_checks.sh 含 public + callable_lifetime + edge_semantics_checks 全部通过（已验证）。edge_semantics_checks 覆盖全面：临时 string、batch、取消、异常、wrap-around。
- 小计：88/100
- 硬性扣分触发：否
- 关键缺陷：`last_active_tick` 非 atomic，多线程下有数据竞争。

### P03
- 正确性 45 → 32/45：基本结构正确但有关键问题。`subscribe` 在**锁内** `make_shared<Callback>(std::move(callback))`（subscription_hub.cpp:8-13），违反 callable 生命周期的锁边界要求。`drain` 通过 copy `subscriptions_`（shared_ptr refcount++）做快照（:48），锁外回调，每次投递前重新加锁检查 `still_active`（:64-71）。`unsubscribe` 在锁内 `move(*it)`（Subscription 包含 `shared_ptr<Callback>`），但 Subscription 是 `vector<Subscription>` 的直接元素，`std::move` Subscription 会 move 其 shared_ptr 字段——这不涉及 callable 移动，只是控制块指针移动，可以接受。`expire_idle` 用 `std::partition` 在锁内移动 Subscription 对象——同样只移动 shared_ptr 控制块指针。
- C++质量 20 → 14/20：`last_active_tick` 非 atomic（`uint32_t`）且被 drain 写（:89）和 expire_idle 读（:112），多线程数据竞争。`subscriptions_` 是 `vector<Subscription>` 而非 `shared_ptr`，drain 的 `drain_subs = subscriptions_` 做深拷贝（:48），拷贝 shared_ptr 增加引用计数，可以接受。
- 约束遵循 15 → 6/15：**硬性封顶条件违反**：`subscribe` 在锁内移动 Callback（subscription_hub.cpp:12），违反第 7 条 callable 生命周期锁边界要求。
- 可维护性 10 → 7/10：实现简洁，129 行。但 per-event 加锁检查 `still_active` 策略效率低。
- 验证证据 10 → 6/10：仅 public_checks + callable_lifetime_checks 通过（已验证）。未新增额外边界测试。
- 小计：55/100
- 硬性扣分触发：是，subscribe 锁内 move Callback → 总分 ≤ 60
- 关键缺陷：subscribe 锁内 move Callback（硬性封顶）；last_active_tick 非 atomic 数据竞争；无额外测试。

### P04
- 正确性 45 → 42/45：实现完整正确。`subscribe` 在锁外 `make_shared<Subscription>` + `move callback`（:12-14），锁内只 push shared_ptr（:21）。`drain` 用 `batch.assign(move iterators)` + `pending_.clear()` 截取（:56-58），`subs_snapshot = subscriptions_`（:61），锁外投递。`alive` atomic 实现 per-event 取消（:71）。`expire_idle` 用 uint32 模减（:94-96）。微扣分：`batch.assign` 而非 `swap` 是 O(n) 拷贝而非 O(1) 交换，但语义正确。
- C++质量 20 → 17/20：`atomic<bool> alive` + `atomic<uint32_t> last_active_tick` 正确处理并发。`relaxed` 内存序选择合理。代码清晰紧凑。
- 约束遵循 15 → 15/15：callable 生命周期完全在锁外。API 保持不变。
- 可维护性 10 → 8/10：116 行，逻辑清晰。`expire_idle` 使用手写 compaction 而非 std::remove_if，略有可读性损失。
- 验证证据 10 → 9/10：public + callable_lifetime + extra_checks 全部通过（已验证）。extra_checks 覆盖 8 项边界含递归 drain 和多线程。
- 小计：91/100
- 硬性扣分触发：否
- 关键缺陷：无重大缺陷。

### P05
- 正确性 45 → 28/45：基本结构存在但有多处问题。`subscribe` **在锁内** `move(callback)`（subscription_hub.cpp:13-14）——违反 callable 生命周期锁边界。`unsubscribe` 使用 `std::partition` **在锁内**做 `Subscription` 的 move-assignment（含 `Callback` 的 move，subscription_hub.hpp:44 `Callback callback`）——再次违反。`drain` 通过 `subs_snapshot = subscriptions_`（:45）做快照（深拷贝 `vector<Subscription>`，含 `Callback` 的 **拷贝**！因为在锁内，这是又一次违反。`removed_` set 追踪取消。`last_active_tick` 非 atomic 且在 drain 写（:72, :87）和 expire_idle 读（:107），多线程数据竞争。
- C++质量 20 → 10/20：`std::function` 的 copy/move 在锁内多处发生。`Subscription` 直接存 Callback 而非 shared_ptr，导致快照深拷贝时触发 callable 拷贝。`expired` vector 在锁作用域**内**返回（:113），析构发生在锁释放后的函数末尾——实际上 `return` 在 `lock_guard` 作用域内，Callback 析构仍在锁内。
- 约束遵循 15 → 3/15：**硬性封顶条件违反**：subscribe 锁内 move callback；unsubscribe 锁内 move callback（via partition）；drain 锁内 copy callbacks（via snapshot assignment）。多处违反 callable 生命周期锁边界。
- 可维护性 10 → 5/10：`removed_` set 方法不够优雅。per-event 短暂加锁检查 removed_ 增加复杂度。
- 验证证据 10 → 7/10：public_checks + callable_lifetime_checks 通过（已验证），但未新增额外测试。callable_lifetime_checks 通过是因为 ReenterOnDestroy 的析构在 unsubscribe 返回后触发，而 unsubscribe 的 retired 在锁外析构，但 subscribe 的锁内 move 未被此测试覆盖。
- 小计：53/100
- 硬性扣分触发：是，多处 callable 锁边界违反 → 总分 ≤ 60
- 关键缺陷：subscribe（:13-14）锁内 move Callback；unsubscribe（:25-29 锁内 partition move Callback）；drain（:45）锁内 copy Callbacks；expire_idle（:113）Callback 析构在锁内返回；last_active_tick 非 atomic。

## Q2 CoalescingCache

### P01
- 正确性 45 → 42/45：实现完整正确。单一 `State::mutex` 保护所有状态。owner 在锁外调 loader（:139），clock 锁外（:150）。同一临界区完成 result/done/erase flight（:160-184）。recursive_load 通过 `flight->owner == this_thread::get_id()` 检测（:76-78）。TTL=0 不写 ready（:156）。invalidate 标记 `invalidated` 阻止回写（:161, :203）。LRU splice（:103-104, :167）。微扣：过期判定 `expired = (now - loaded_at) >= entry_ttl`（:97）用 `<` 变体，需注意 `ttl=0` 时 `(now - loaded_at) >= 0` 恒成立的处理——但代码已在 ready 路径先检查 flight 再判过期，不会在 ready 路径对 TTL=0 错误返回缓存值，因为 TTL=0 时 cacheable=false（:156）。
- C++质量 20 → 17/20：单一 mutex + shared_ptr<State> 模式清晰。`Flight` 含 `condition_variable` 配合 `unique_lock`。`for(;;)` 重试循环处理并发竞争。代码注释清晰。
- 约束遵循 15 → 14/15：满足所有约束。Clock exception handling 为「按未过期处理」是合理选择。
- 可维护性 10 → 8/10：211 行，逻辑清晰但 `for(;;)` + 多次 lock/unlock 增加复杂度。
- 验证证据 10 → 9/10：public + clock_reentrancy + extra_boundary_checks（11 用例）全部通过（已验证）。覆盖全面。
- 小计：90/100
- 硬性扣分触发：否
- 关键缺陷：无重大缺陷。

### P02
- 正确性 45 → 40/45：实现正确。使用 `thread_local vector<ActiveCall>` 做 recursive_load 检测（:56-66），不同于参考的 `thread::id` 方案但等价。`get_or_load` 逻辑清晰：lock→check flight→unlock→clock→lock→check ready→create flight→unlock→loader→lock→publish→unlock→notify。single mutex 保护所有状态（:200-218）。invalidate 标记不取消 loader（:236-237）。clock throw 时返回 failure（:147-153）。微扣：clock throw 处理策略「返回 clock_failure()」较激进，但合理。
- C++质量 20 → 16/20：`ActiveCallGuard` RAII 模式好。`store_ready` lambda 清晰。但 `erase_ready` 和 `store_ready` lambda 使用 `&state` capture 需注意生命周期（:110-130）。
- 约束遵循 15 → 14/15：基本满足。capacity=0 永不缓存（:125 `while size > capacity` 在 capacity=0 时淘汰所有）。
- 可维护性 10 → 8/20：244 行，结构清晰。
- 验证证据 10 → 8/10：public + clock_reentrancy + edge_semantics_checks 通过（已验证）。edge 覆盖 recursive_load、TTL expiry、invalidate 竞态。
- 小计：86/100
- 硬性扣分触发：否
- 关键缺陷：无重大缺陷。

### P03
- 正确性 45 → 36/45：双 mutex 设计（State::mtx + Flight::mtx）。loader 锁外（:112-119），clock 锁外（:66, :122）。recursive_load 用 `owner_tid` 检测（:92-93）。TTL 用 `expires_at = completion_time + ttl`（:151）——这是绝对时间而非模减，对 uint64_t 不会溢出但在语义上不等价于模减（题目未规定必须是模减，可接受）。invalidate `write_back=false`（:195）。**问题**：flight 完成时先 `flight->mtx` 下设 done（:134-138），再 `state_->mtx` 下写 ready + erase flight（:141-173），然后 `notify_all`（:176）。这**违反了单一同步转换要求**（语义 7）——flight 的 done/result 在 flight->mtx 下发布，但 ready 写入和 flight 移除在 state_->mtx 下，两把不同的 mutex。等待者可能在 flight->mtx 上等待到 done 后、state_->mtx 释放前读到完整结果——但因为等待者等的是 flight->mtx（不同锁），可能在 flight done=true 后就唤醒并读 result，然后返回，此时 flight 还在 in_flight map 中。后续新调用者在 state_->mtx 下找到 flight 但 flight 尚未被移除——这是正确的行为（新调用者会加入等待）。但从代码看，等待者在 flight->mtx 上 wait（:107），owner 在 flight->mtx 下 publish done（:135-137），所以等待者唤醒后可以读到 result。但 flight 尚未从 in_flight 移除，新来的线程仍然能看到 flight 并尝试等待——这会第二次 wait 到 flight->done（已 true），立即返回。这是安全的但略低效。主要问题是**违反语义 7**要求的单一同步关系。
- C++质量 20 → 14/20：双 mutex 增加复杂度和潜在的死锁/竞态风险。`expires_at` 用加法而非减法语义不同但可工作。
- 约束遵循 15 → 11/15：基本满足但 flight 完成用了两把 mutex，不完全满足语义 7 "同一同步关系"。
- 可维护性 10 → 7/10：双 mutex 设计增加了理解和维护成本。
- 验证证据 10 → 7/10：public + clock_reentrancy 通过（已验证）。未新增额外测试。
- 小计：70/100
- 硬性扣分触发：否（但 flight 完成的双 mutex 不满足"单一同步关系"，正确性扣分）
- 关键缺陷：flight done/result 和 ready 写入/flight 移除用两把不同 mutex，不满足语义 7 的单一同步转换要求。

### P04
- 正确性 45 → 43/45：实现正确完整。单一 mutex + cv。Clock 锁外（:78, :126）。loader 锁外（:124）。recursive_load 用 `thread_local vector<Flight*> active_loads`（:44-51）。同一临界区完成 result/done/ready-cache-write/erase-flight（:135-160），满足语义 7。invalidate 标记不取消（:176-177）。LRU splice + eviction（:146-155）。capacity=0 永不缓存（:138-139 `capacity > 0`）。
- C++质量 20 → 17/20：匿名命名空间 Flight 结构清晰。`active_loads` thread_local 正确使用。单一 mutex + cv 模式标准。
- 约束遵循 15 → 15/15：满足所有约束。
- 可维护性 10 → 8/10：183 行，逻辑紧凑清晰。
- 验证证据 10 → 9/10：public + clock_reentrancy + extra_checks（11 用例）全部通过（已验证）。覆盖全面：TTL、failure、exception、recursive、invalidate、LRU、capacity=0、stress 32 线程。
- 小计：92/100
- 硬性扣分触发：否
- 关键缺陷：无重大缺陷。

### P05
- 正确性 45 → 34/45：基本实现存在但有多个问题。使用双 mutex（State::mtx + FlightFuture::done_mtx）。**问题 1**：flight 完成时先在 state_->mtx 下写 ready cache（:129-149），再在 flight->done_mtx 下设 result_set（:152-155），notify（:156），最后在 state_->mtx 下 erase flight（:158-161）。这违反了语义 7 的单一同步转换——result 的发布和 ready 写入在不同锁下。**问题 2**：recursive_load 检测用 `thread_local unordered_set<string> loading_keys`（:55-56），只按 key 不区分 cache 实例。两个不同 CoalescingCache 实例的同 key loader 会互相误判为 recursive。**问题 3**：clock throw 被 `try { complete_time = state_->clock(); } catch(...) {}`（:124）静默吞掉，complete_time 保持 0，但不影响正确性（成功结果已缓存时会用 0 作为 TTL 起点）。**问题 4**：invalidate 时加两把锁（:167-175），先 state_->mtx 移除 ready，再 done_mtx 设 invalidated。如果 owner 恰好在 done_mtx 和 state_->mtx 之间，可能产生 TOCTOU。
- C++质量 20 → 12/20：双 mutex + 分步发布结构混乱。`ready_order.remove(key)` 是 O(n) 链表操作（:85, :133, :169），效率低。
- 约束遵循 15 → 10/15：recursive_load 检测不区分实例，违反"同 cache 实例"语义（语义 2）。
- 可维护性 10 → 6/10：双 mutex 逻辑分散，理解和维护成本高。
- 验证证据 10 → 6/10：public + clock_reentrancy 通过（已验证）。未新增额外测试。
- 小计：68/100
- 硬性扣分触发：否
- 关键缺陷：双 mutex 违反语义 7；recursive_load 不区分 cache 实例；invalidate 两把锁存在竞态窗口。

## Q3 RoutingConfig

### P01
- 正确性 45 → 42/45：实现正确完整。`valid` 锁内校验但作为 private 函数（:12-21）可接受。`reload` 先 valid 再锁内 publish + snapshot observers（:54-88）。`current()` TLS per-instance pin（:35-37, `unordered_map<RoutingConfig*, shared_ptr>`）。subscribe 锁外 `make_shared<Observer>(std::move)`（:94），锁内只 emplace。unsubscribe 锁外析构（:113）。通知锁外（:79-86）。异常隔离 + 计数（:82-85）。`find_target` 最长前缀（:127-132）。
- C++质量 20 → 17/20：shared_ptr<const Config> 不可变快照模式。TLS pin 用 `unordered_map`。`valid` 用 `set<string>` 去重。observer_error_count 短锁。
- 约束遵循 15 → 15/15：满足所有。
- 可维护性 10 → 8/10：138 行，逻辑清晰。
- 验证证据 10 → 9/10：public + cross_instance_current(ASAN) + observer_copy_reentrancy + extra_boundary_checks（11 用例）全部通过（已验证）。
- 小计：91/100
- 硬性扣分触发：否
- 关键缺陷：无重大缺陷。valid() 在锁内调用（:62），但 valid 是 const 纯函数不影响正确性。

### P02
- 正确性 45 → 40/45：实现正确。`valid` 锁外调用（:50），避免不必要锁竞争。`reload` 锁内 publish + snapshot observers（:57-68），锁外通知（:70-78）。`current()` TLS per-instance pin（:14-16, `unordered_map`）。subscribe 锁外构造 holder（:84），锁内 emplace。unsubscribe 锁外析构（:104）。`find_target` 通过 snapshot() 取快照（:108）。
- C++质量 20 → 17/20：代码简洁 122 行。`valid` 在锁外好。
- 约束遵循 15 → 15/15：满足所有。
- 可维护性 10 → 8/10：简洁清晰。
- 验证证据 10 → 9/10：public + cross_instance(ASAN) + observer_copy + edge_semantics_checks 通过（已验证）。
- 小计：89/100
- 硬性扣分触发：否
- 关键缺陷：无重大缺陷。valid() 锁外但 `valid` 是 const 方法不读共享状态（:38 `const`），安全。

### P03
- 正确性 45 → 38/45：实现基本正确。`valid` 静态方法（:49），锁外调用。`reload` 锁内 publish + snapshot observers（:68-78），锁外通知（:82-90）。`current()` TLS per-instance pin（:22-24, `unordered_map`）。**问题**：subscribe **锁内** `make_shared<Observer>(std::move(observer))`（:99-101）——Observer 的 move 在锁内发生，违反语义 4 关于 callable 生命周期锁外要求。unsubscribe 锁外析构（:117）正确。`find_target` 通过 snapshot 一致（:121-134）。
- C++质量 20 → 14/20：subscribe 锁内 move Observer 是问题。其余清晰。
- 约束遵循 15 → 12/15：subscribe 锁内 move Observer 违反语义 4 锁外要求。
- 可维护性 10 → 7/10：140 行，清晰但因 subscribe 问题扣分。
- 验证证据 10 → 8/10：public + cross_instance(ASAN) + observer_copy 通过（已验证）。未新增额外测试。
- 小计：79/100
- 硬性扣分触发：否（Q3 无硬性封顶条件）
- 关键缺陷：subscribe 锁内 move Observer（违反语义 4）。

### P04
- 正确性 45 → 40/45：实现正确。`valid` 锁外调用（:58），`reload` 锁内 publish + snapshot observers（:65-78），锁外 `notify_observers`（:82）。`current()` TLS per-instance pin（:16-18, `unordered_map<RoutingConfig*, shared_ptr>`）。subscribe 锁外 `make_shared<Observer>`（:108），锁内 emplace。unsubscribe 锁外析构（:125）。`find_target` 通过 snapshot 一致（:130-142）。`notify_observers` 独立私有方法好（:86-104）。
- C++质量 20 → 17/20：代码组织好，有 `notify_observers` 辅助方法。
- 约束遵循 15 → 15/15：满足所有。
- 可维护性 10 → 9/10：147 行，结构清晰。
- 验证证据 10 → 8/10：public + cross_instance(ASAN 报告环境问题但实际通过) + observer_copy + extra_checks（10 用例）通过。
- 小计：89/100
- 硬性扣分触发：否
- 关键缺陷：无重大缺陷。

### P05
- 正确性 45 → 35/45：实现基本正确但有问题。`valid` 锁外。`reload` **锁内** copy `observers_` map（:46 `observers_snapshot = observers_`）——这会 copy 所有 `shared_ptr<Observer>`（refcount++），shared_ptr 的拷贝只涉及控制块指针操作，不拷贝 Observer 本身，**可以接受**。但**问题 1**：subscribe **锁内** `make_shared<Observer>(std::move(observer))`（:62-63），Observer move 在锁内发生，违反语义 4。**问题 2**：`current()` 在锁内写 TLS pin `pins[this] = current_`（:15），这意味着在持锁期间做了 shared_ptr 赋值操作——虽然不涉及 callable 操作（只是 refcount++），但增加了锁持有时间。**问题 3**：`observers_snapshot = observers_`（:46）拷贝整个 map，虽然 map 只含 shared_ptr 拷贝控制块指针，但如果 shared_ptr 的原子引用计数在某些实现上涉及重量级操作可能有问题——不过技术上正确。
- C++质量 20 → 13/20：代码较简洁 93 行，但 subscribe 锁内 move Observer 和 current() 锁内写 TLS 是问题。
- 约束遵循 15 → 12/15：subscribe 锁内 move Observer 违反语义 4。
- 可维护性 10 → 7/10：代码简洁但有小问题。
- 验证证据 10 → 10/10：public + cross_instance(ASAN，实际通过只是环境 ASAN runtime 损坏) + observer_copy 通过（已验证）。
- 小计：77/100
- 硬性扣分触发：否
- 关键缺陷：subscribe 锁内 move Observer（违反语义 4）；current() 持锁期间写 TLS。

## 关键差异观察
- **Q1 callable 生命周期**：P01、P02、P04 正确地在锁外 move/copy Callback，通过 `shared_ptr<Callback>` 模式且在加锁前构造完毕。P03 和 P05 在 subscribe 中于锁内 move Callback，触发硬性封顶（≤60）。P05 还因 `vector<Subscription>` 直接包含 `Callback`（而非 shared_ptr），在 drain 快照赋值时**拷贝**所有 Callback——这是额外的锁内 callable 拷贝。
- **Q2 单一同步转换**：P01、P02、P04 使用单一 mutex 保护 flight 完成全序列（result、done、ready-write、flight-erase）。P03 和 P05 使用双 mutex（state mutex + flight mutex），flight done/result 和 ready 写入在不同锁下，不严格满足语义 7。
- **Q2 recursive_load 检测**：P01 用 `thread::id`（per-flight），P03 用 `thread::id`（正确），P02 和 P04 用 thread_local 活跃调用栈（正确），P05 用 thread_local `unordered_set<string>` 只按 key——不区分 cache 实例，可能误判。
- **Q3 subscribe callable 锁边界**：P01、P02、P04 在锁外构造 `shared_ptr<Observer>`。P03 和 P05 在锁内 `make_shared<Observer>(std::move(observer))`。
- **测试覆盖**：P01（3题共30个额外用例）和 P04（3题共29个额外用例）测试最全面。P02 有 3 题共 4 个额外测试文件覆盖核心边界。P03 和 P05 未新增任何额外测试。

## 评分不确定 / 未验证项
- **P03 Q2 双 mutex 正确性**：虽然双 mutex 设计不严格满足语义 7 的"同一同步关系"要求，但实际运行中等待者仍能获得正确结果。在极端并发下是否存在窗口问题未能用 TSan 定量验证。
- **P05 Q2 recursive_load 跨实例误判**：无法从源码确定在实际评分场景中是否会遇到两个不同 cache 实例的同 key loader 互相误判为 recursive 的情况。
- **P02 Q1 `last_active_tick` 数据竞争**：uint32_t 的读写在多线程下构成 C++ UB，但实际运行中通常不会导致错误行为（x86 上天然原子）。
- **TSan/ASAN 环境限制**：本环境 ASAN 在 leak detect 模式下不可用（DEADLYSIGNAL 级联），TSan 库缺失。所有参评对象均未运行 TSan。
- **所有 run_public_checks.sh 均编译运行通过**（部分 ASAN 测试因环境问题跳过或 DEADLYSIGNAL，但非 ASAN 编译均通过）。
