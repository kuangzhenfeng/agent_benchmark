# 盲评评分报告 — scorer B

> 由独立 scorer subagent B 输出，仅基于匿名 ID P01–P05。主 agent 收到后原样归档，未修改。
>
> 评分环境：g++ 9.4.0 -std=c++17，环境 WSL2。所有 run_public_checks.sh 均实跑；另跑多组自写探针。ASAN 单测偶发 DEADLYSIGNAL 级联（脚本字节相同重跑 PASS），判定为环境 ASAN/LSan 抖动。

## 总分表

| Pxx | Q1/100 | Q2/100 | Q3/100 | 总分/300 |
|-----|--------|--------|--------|----------|
| P01 | 82 | 95 | 96 | 273 |
| P02 | 93 | 96 | 95 | 284 |
| P03 | 88 | 90 | 92 | 270 |
| P04 | 93 | 95 | 95 | 283 |
| P05 | 58 | 74 | 90 | 222 |

## Q1 SubscriptionHub

### P01
- 正确性 45 → 36/45：自有存储、批次 swap、锁外回调、异常隔离、uint32 模减过期（is_expired src:12-19）均正确；run_public_checks + 自写 extra_boundary_checks（8 用例）+ 多线程压测通过。关键缺陷：unsubscribe（src:41-58）从未置 cancelled=true，cancelled 原子只在初始化为 false（src:31）后从不被写。drain 的 audience 是 shared_ptr 快照，取消后仍保活 → 自取消的订阅者仍会收到当前事件之后的后续事件。探针证实：回调内 unsubscribe(self) 后 delivered=2，违反语义 4。ANSWER.md 自称"cancelled 检查使其不再接收"，与实现不符（伪修复）。其自写测试 test_cancel_in_callback 反而 assert(delivered==2)，把错误行为固化。
- C++质量 20 → 18/20
- 约束遵循 15 → 15/15
- 可维护性 10 → 9/10
- 验证证据 10 → 8/10：自写测试覆盖面广、ASan 通过；但 TSan 未跑，且关键自写测试编码了错误语义。
- 小计：86（正确性缺陷已在维度体现，无额外封顶）
- 调整后小计：82/100
- 硬性扣分触发：否。callable 复制/移动/销毁均在锁外。
- 关键缺陷：cancelled 从未置位 → 回调内取消不能阻止后续事件投递。

### P02
- 正确性 45 → 42/45：自有存储、shared_ptr<Callback>、drain 锁内 swap+按 subscription_cutoff 逐事件重快照（src:64-103）、异常隔离（delivered 计含抛异常、callback_errors 单列）、uint32 模减过期（src:113）均正确。逐事件重查 subscriptions_ 使取消的订阅者不再收到后续事件（探针：自取消 delivered=1）。回调内新订阅因 id>cutoff 被排除。expire_idle 析构在锁外。轻微：每次投递前后都重取锁。
- C++质量 20 → 18/20
- 约束遵循 15 → 15/15
- 可维护性 10 → 9/10
- 验证证据 10 → 9/10
- 小计：93/100
- 硬性扣分触发：否。
- 关键缺陷：无重大；逐事件重锁属风格选择。

### P03
- 正确性 45 → 40/45：自有存储、shared_ptr<Callback>、drain 锁内 swap+拷 drain_subs、逐事件重查 still_active（src:62-74）正确处理取消；异常隔离、uint32 模减正确。未新增任何边界测试（ANSWER 自述"时间限制"），仅 public+callable_lifetime。逐事件 any_of 线性重查 O(n²) 但功能正确。
- C++质量 20 → 17/20：subscribe 在锁内 make_shared<Callback>(move)——见约束讨论。
- 约束遵循 15 → 14/15：subscribe（src:8-15）在持锁期间 std::make_shared<Callback>(std::move(callback))，即 std::function 的移动发生在 mutex 内。Q1 硬性封顶条件针对 callable 生命周期的锁边界——严格按字面这是把 callable 移动放在锁内，但 std::function 移动通常不执行用户代码（探针：function 移动重入不死锁）。判为"轻微违背语义 7 措辞、无实际可观测危害"，小幅扣分，不触发 60 封顶。
- 可维护性 10 → 8/10：O(n²) 重查，未补测试。
- 验证证据 10 → 6/10：仅公开检查通过，无自写边界/并发测试，无 ASan/TSan。
- 小计：88/100
- 硬性扣分触发：否（不构成可观测的锁内 callable 用户代码）。
- 关键缺陷：锁内移动 std::function（措辞违规、无实际危害）；无自写测试。

### P04
- 正确性 45 → 42/45：自有存储、shared_ptr<Subscription>+atomic<bool> alive、drain 锁内 assign(move)+clear 截批并拷订阅快照（src:49-83）、逐事件查 alive、异常隔离、uint32 模减（src:94-96）均正确。取消语义探针通过（delivered=1）。回调内新订阅不在快照。析构锁外。expire_idle 用手写双指针分区，正确。
- C++质量 20 → 19/20
- 约束遵循 15 → 15/15
- 可维护性 10 → 9/10
- 验证证据 10 → 8/10：public + extra（8 用例）+ ASan（detect_leaks=0）通过；无大规模并发压测、无 TSan。
- 小计：93/100
- 硬性扣分触发：否。
- 关键缺陷：无重大。

### P05
- 正确性 45 → 20/45：自有存储、drain 锁内 swap+快照、异常隔离、uint32 模减（src:103-114）基本对；取消用 removed_ 集合逐事件短锁重查（探针：自取消 delivered=1，正确）。致命缺陷：Subscription 按值持有 Callback callback（include:44），drain 在锁内 subs_snapshot = subscriptions_（src:45，位于 lock_guard 块内）拷贝整个 vector 即拷贝每个 std::function；subscribe 也在锁内 sub.callback = std::move(callback)（src:13）。探针"拷贝构造重入 hub 的 callable"对 P05 确定性死锁（timeout 124），其余四个均通过。
- C++质量 20 → 12/20：未用 shared_ptr 托管 callable，导致必须拷贝 std::function。
- 约束遵循 15 → 8/15：违反 Q1 硬性封顶条件。
- 可维护性 10 → 7/10
- 验证证据 10 → 6/10
- 调整前小计：53；硬性封顶至 60 后，因正确性缺陷取较低者 → 58/100
- 硬性扣分触发：是（Q1 总分 ≤ 60）。drain 在 mutex_ 内复制 std::function（src:45）、subscribe 在 mutex_ 内移动 std::function（src:13），直接违反语义 7（硬性封顶条件）。探针实测死锁。
- 关键缺陷：锁内拷贝/移动 std::function → callable 特殊成员重入死锁（硬性封顶）。

## Q2 CoalescingCache

### P01
- 正确性 45 → 43/45：单 State::mutex 保护 ready LRU + in-flight；loader/Clock 全在锁外；owner 在一次临界区内完成 result/done/cacheable 决定/erase（src:159-186）满足语义 7；递归用 flight->owner==thread::id（src:76-79）；invalidate 置 invalidated 不取消 loader（src:199-204）；TTL=0/capacity=0 不缓存但合并。探针：16 线程合并 loader=1、invalidate-在途不回写、递归返回 recursive_load 全通过。轻微：ready 命中路径释放锁查 Clock 再重取，用 loaded_at 比对 + continue 重试。
- C++质量 20 → 19/20
- 约束遵循 15 → 15/15
- 可维护性 10 → 9/10
- 验证证据 10 → 9/10：public + extra（11 用例）+ ASan 通过；无 TSan。
- 小计：95/100
- 硬性扣分触发：否。
- 关键缺陷：无重大。

### P02
- 正确性 45 → 43/45：单 State::mutex；thread_local (state,key) 活跃调用栈做递归检测（src:51-99，正确按实例区分）；loader/Clock 锁外；owner 单临界区发布（src:200-219）；invalidate 置 invalidated；TTL/capacity=0 不缓存但合并；LRU splice。探针全通过，包括跨实例同 key 嵌套（inner 正常 ok）。
- C++质量 20 → 19/20：ActiveCallGuard RAII 清理优雅。
- 约束遵循 15 → 15/15
- 可维护性 10 → 9/10
- 验证证据 10 → 9/10
- 小计：96/100
- 硬性扣分触发：否。
- 关键缺陷：无重大。

### P03
- 正确性 45 → 38/45：ready LRU + in-flight；loader/Clock 锁外；递归用 flight->owner_tid（src:92，按 flight 即按实例，正确）；invalidate 置 write_back=false（src:195）。设计偏离语义 7：用两把锁——flight->mtx 发布 result/done，state_->mtx 写 ready+erase。等待者经 flight->mtx+cv 同步结果——实际同步正确（done/result 在 flight->mtx 下写读，cv 配对），属行为等价，仅在"单一同步点"措辞上偏离。
- C++质量 20 → 17/20：两把锁增加复杂度。
- 约束遵循 15 → 14/15
- 可维护性 10 → 8/10：双锁状态机较难审。
- 验证证据 10 → 7/10：仅 public + clock_reentrancy 通过，无自写并发/边界测试；无 ASan/TSan。
- 小计：90/100
- 硬性扣分触发：否。
- 关键缺陷：两锁设计偏离"单一同步点"（行为等价、同步正确）；无自写测试。

### P04
- 正确性 45 → 43/45：单 State::mutex+单 cv；thread_local vector<Flight*> 按 flight 身份做递归检测（src:44-51，正确）；loader/Clock 锁外；owner 单临界区发布（src:135-161）；invalidate 置 invalidated；LRU splice + 尾淘汰；capacity=0 经 cacheable 跳过。探针全通过，含跨实例同 key 嵌套、32 线程压测 loader=1。
- C++质量 20 → 19/20：注释明确解释"无第二把 mutex"以满足语义 7。
- 约束遵循 15 → 15/15
- 可维护性 10 → 9/10
- 验证证据 10 → 9/10：public + extra + ASan(detect_leaks=0) 通过；无 TSan。
- 小计：95/100
- 硬性扣分触发：否。
- 关键缺陷：无重大。

### P05
- 正确性 45 → 30/45：基本状态机在（loader/Clock 锁外、invalidate 置 invalidated、LRU、capacity=0 经 push-then-evict 功能正确、TTL=0 不缓存）。关键缺陷：递归检测用 thread_local unordered_set<string> tl_loading_keys（src:55），只按 key、不按实例。探针：在实例 a 的 loader 内对另一实例 b 同 key "k" 嵌套调用 → P05 错误返回 recursive_load（status=2），而 P01–P04 均正确返回 ok。这正是参考解答警示的"单个全局 TLS pin 被跨实例覆盖"类伪修复。另：发布用独立 future->done_mtx（两锁模式）。
- C++质量 20 → 15/20：ready_order.remove(key) O(n)；should_cache 漏判 capacity>0；两锁设计。
- 约束遵循 15 → 13/15
- 可维护性 10 → 8/10
- 验证证据 10 → 8/10：public + clock_reentrancy 通过；无自写并发/边界测试；ANSWER 风险描述笼统（未提跨实例递归）。
- 小计：74/100
- 硬性扣分触发：否（Q2 无硬性封顶条件）。
- 关键缺陷：递归检测不按实例 → 跨实例同 key 嵌套误返 recursive_load（参考解答明示的伪修复模式）。

## Q3 RoutingConfig

### P01
- 正确性 45 → 43/45：shared_ptr<const Config> published_ 原子发布（锁内 swap，src:67）；valid 全量校验失败即返 false 不动状态（src:62-64）；版本严格 +1；current() 用 thread_local unordered_map<const RoutingConfig*, shared_ptr<const Config>> 按 this 分槽（src:35-46，跨实例不互覆盖，探针/ASan 通过）；observer shared_ptr<Observer>，reload 锁内仅拷指针、锁外通知（src:71-86）；异常隔离；find_target 最长前缀。subscribe/unsubscribe callable 移动/销毁在锁外（src:94、107-113）。
- C++质量 20 → 19/20
- 约束遵循 15 → 15/15
- 可维护性 10 → 9/10
- 验证证据 10 → 10/10：public + cross_instance(ASan) + observer_copy + extra（11 用例）+ clang 交叉 + ASan 通过；风险说明详尽。
- 小计：96/100
- 硬性扣分触发：否。
- 关键缺陷：无重大。

### P02
- 正确性 45 → 42/45：shared_ptr<const Config> current_ 原子发布；锁外校验；版本 +1；current() 文件作用域 thread_local map<this,...>（src:14-31，跨实例 OK，ASan 通过）；observer shared_ptr，reload 锁内拷指针锁外通知（src:64-67）；异常隔离；find_target 经 snapshot() 一致快照最长前缀。subscribe 锁外构造 shared_ptr<Observer>（src:84）。
- C++质量 20 → 18/20
- 约束遵循 15 → 15/15
- 可维护性 10 → 9/10
- 验证证据 10 → 9/10：public + cross_instance(ASan) + observer_copy + edge 通过；自述 clang++ 交叉；无 TSan。
- 小计：95/100
- 硬性扣分触发：否。
- 关键缺陷：无重大。

### P03
- 正确性 45 → 41/45：shared_ptr<const Config> current_snapshot_ 原子发布；锁外 valid；版本 +1；current() TLS map<this,...>（src:22-42，跨实例 OK，ASan 通过）；observer shared_ptr，reload 锁内拷指针锁外通知（src:74-77）；异常隔离；find_target 一致快照最长前缀。轻微违背语义 4 措辞：subscribe（src:98-103）在 lock_guard 内 observers_.emplace(id, std::make_shared<Observer>(std::move(observer)))，即 std::function 的移动发生在锁内。实测对 SBO 目标 std::function 移动不触发用户移动构造（探针不死锁），无可观测危害。
- C++质量 20 → 17/20
- 约束遵循 15 → 13/15：上述锁内移动 std::function（措辞违规、无实际危害）。
- 可维护性 10 → 8/10：无自写边界测试（ANSWER 自述时间限制）。
- 验证证据 10 → 7/10：仅 public + cross_instance(ASan) + observer_copy 通过；无自写测试、无 TSan。
- 小计：92/100
- 硬性扣分触发：否。
- 关键缺陷：锁内移动 observer std::function（措辞违规、无危害）；无自写测试。

### P04
- 正确性 45 → 43/45：shared_ptr<const Config> current_ 原子发布；锁外 valid；版本 +1；current() TLS map<this,...>（src:16-38，跨实例 OK，ASan 通过）；observer shared_ptr，reload 锁内拷指针、独立 notify_observers 锁外通知（src:82-104）；异常隔离；find_target 一致快照最长前缀；subscribe/unsubscribe callable 移动/销毁锁外（src:108、121-123）。保留 caller version=0（src:23-25 注释）。
- C++质量 20 → 19/20：notify_observers 抽取为私有方法，结构清晰。
- 约束遵循 15 → 15/15
- 可维护性 10 → 9/10
- 验证证据 10 → 9/10：public + cross_instance(ASan) + observer_copy + extra 通过，g++/clang++ 交叉；自述本机 ASAN 运行时损坏改用交叉验证（诚实说明，未编造）。
- 小计：95/100
- 硬性扣分触发：否。
- 关键缺陷：无重大。

### P05
- 正确性 45 → 41/45：shared_ptr<const Config> current_ 原子发布；锁外 valid；版本 +1；current() TLS map<const void*,...> 按实例（src:11-18，跨实例 OK，ASan 通过）；observer shared_ptr，reload 锁内 observers_snapshot = observers_（拷 shared_ptr 不拷 function，src:46）锁外通知；异常隔离；find_target 经 snapshot() 最长前缀。轻微违背语义 4 措辞：subscribe（src:61-65）锁内 make_shared<Observer>(move) 移动 std::function（同 P03，无实际危害）。
- C++质量 20 → 18/20：map<const void*,...> 用 void* key 不如 const RoutingConfig* 贴切，但等价。
- 约束遵循 15 → 13/15：锁内移动 observer std::function（措辞违规、无危害）。
- 可维护性 10 → 8/10：无自写边界测试（ANSWER 风险描述笼统）。
- 验证证据 10 → 7/10：public + cross_instance(ASan) + observer_copy 通过；无自写测试、无 TSan。
- 小计：90/100
- 硬性扣分触发：否。
- 关键缺陷：锁内移动 observer std::function（措辞违规、无危害）；无自写测试。

## 关键差异观察（基于匿名 ID，不外推身份）

1. callable 生命周期锁边界（Q1 硬性封顶）：P01/P02/P04 用 shared_ptr 托管 callable，drain 锁内只动控制块；P03 拷贝 Subscription{shared_ptr<Callback>}（锁内只拷指针）；P05 按值持有 Callback，drain 锁内拷贝整 vector 的 std::function——这是唯一触发 Q1 ≤ 60 封顶的实现，且 callable 拷贝重入探针确定性死锁。
2. Q1 取消语义：P02/P03/P04/P05 逐事件重查订阅是否仍在 → 正确跳过取消者的后续事件；P01 有 cancelled 原子字段但 unsubscribe 从不置位，自取消后仍投递后续事件（违反语义 4），其自写测试还把错误行为 assert 固化。
3. Q2 递归检测的实例隔离：P01/P03 用 flight->owner==thread::id（天然按实例），P02 用 (state,key)，P04 用 per-flight 指针栈——均正确；P05 用全局 thread_local unordered_set<string>（只按 key），跨实例同 key 嵌套误返 recursive_load（参考解答明示的伪修复）。
4. Q2 单一同步点：P01/P02/P04 单 mutex 覆盖 ready+flight+result；P03/P05 用双锁（flight 自带 mutex），但均经 flight-mutex+cv 正确同步结果，行为等价，仅措辞偏离。
5. 测试自觉性：P01（每题 8–11 用例 + ASan + clang 交叉 + 详尽风险）、P04（8–12 用例 + ASan）、P02（edge 用例 + clang）验证意识强；P03/P05 仅依赖公开检查，无自写边界/并发测试，ANSWER 风险描述偏笼统。
6. Q3 跨实例 current()：五者均用按 this 分槽的 TLS map，均正确通过 cross_instance ASan。P03/P05 的 subscribe 锁内移动 observer std::function（措辞违规、无实际危害）。
7. 脚本完整性：无任何 Pxx 削弱 run_public_checks.sh；P02/P04 各增补自写测试入脚本。

## 评分不确定 / 未验证项

1. TSan 全员未跑：五者均无 ThreadSanitizer 记录（自述 toolchain 缺 libtsan_preinit.o 或运行时损坏）。以"多线程压测 + ASan + 锁覆盖分析"替代，并发数据竞争属"未验证风险"，已在各 Pxx"验证证据"维度扣减；未臆测 TSan 通过。
2. P01/P02 q03 脚本的 ASAN DEADLYSIGNAL 级联：脚本字节与 P03/P04 完全相同却偶发级联（重跑又 PASS），且直接命令行运行 ASAN 干净通过，判定为 WSL2 下 ASAN/LSan 环境抖动，未计为代码缺陷。
3. P03/P05 Q2 双锁设计的内存序：分析认为 flight-mutex+cv 已正确同步 result，并发探针（16 线程 ×3 轮、200 次 invalidate 竞态）均未复现错配，但缺乏 TSan 定量佐证，标记为"设计偏离、未复现缺陷"。
4. P01 Q1 cancelled 缺陷的影响范围：仅影响"回调内 unsubscribe 同一订阅者后该订阅者在同一批次后续事件"这一组合；其他取消不受影响。判为中等正确性缺陷（非整体不可用）。
5. P05 Q2 should_cache 漏判 capacity>0：靠 push-then-evict 兜底，功能正确但瞬时 size=1 且冗余，仅在 C++ 质量/可维护性小幅体现，未单列正确性扣分。
6. P03/P05 Q3 锁内移动 observer std::function：仅按字面措辞轻微扣分；因 std::function 移动 SBO 目标不触发用户代码、探针不死锁，未按硬性封顶处理（Q3 亦无该封顶）。
