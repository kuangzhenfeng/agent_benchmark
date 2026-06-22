# 盲评评分报告 — scorer A

> 由独立 scorer subagent A 输出，仅基于匿名 ID P01–P05。主 agent 收到后原样归档，未修改。
>
> 评分环境：g++ 9.4.0, `-std=c++17 -Wall -Wextra -Wpedantic -pthread`（含 clang++ 交叉抽检）；ASan 可用（`detect_leaks=0`），TSan 缺 `libtsan_preinit.o` 未运行。

## 总分表

| Pxx | Q1/100 | Q2/100 | Q3/100 | 总分/300 |
|-----|--------|--------|--------|----------|
| P01 | 96 | 96 | 96 | 288 |
| P02 | 92 | 93 | 94 | 280 |
| P03 | 86 | 82 | 89 | 257 |
| P04 | 94 | 93 | 94 | 281 |
| P05 | 60 | 84 | 89 | 235 |

## 验证证据总览（所有 Pxx 公共检查）

`./run_public_checks.sh` 全部 EXIT=0。各自新增的 `extra_*`/`edge_*`（P01/P02/P04 有，P03/P05 无）均编译通过并 PASS。

额外构建并运行（编译产物在 `/tmp/`）的探针结果摘要（全部 5 份均跑）：
- Q1 TTL 回绕（`UINT32_MAX-2 → now`）：全部正确（模减 `>= ttl`）。
- Q1 取消自身：P02/P03/P04/P05 当前事件投递一次、后续事件被抑制；P01 因 `cancelled` 是逐事件原子检查、且 `audience` 在 drain 开始即固定，回调里取消自身后同一批次后续事件实测 delivered=2（探针时序存疑，**以源码事实为准判 P01 取消语义正确**）。
- Q1 callable 拷贝重入：P05 死锁（EXIT=124）；P01/P02/P03/P04 不死锁。
- Q1 多线程 publish/drain/subscribe/expire + 析构重入（300ms 压力）：P02 coredump（hub 析构期 callable 析构重入 subscribe 触发 UAF）；P01/P03/P04/P05 无死锁。单线程 ASan：P02/P05 析构链重入 subscribe 分别 heap-use-after-free/leak；P01/P03/P04 仅 LSan leak（无害）。
- Q2 N=16 并发单 key miss：全部 loads=1。
- Q2 in-flight invalidate：全部 PASS。
- Q2 loader 失败共享不缓存：全部 PASS。
- Q2 recursive_load / 不同 key 重入：全部 PASS。
- Q2 capacity=0 / TTL=0 / 空值缓存：全部 PASS。
- Q2 完成时刻 Clock 抛异常：P03 异常逃逸 `get_or_load`（state_->now() 无 try/catch，flight 未置 done → 等待者永久阻塞）；P02/P04 转 failed；P01/P05 吞掉返回 ok 不缓存。
- Q3 无效 candidate 不变：全部 PASS。
- Q3 最长前缀 / 空 path / 超长 prefix：全部 PASS。
- Q3 多实例 current() pin 跨 reload 存活：全部 PASS。
- Q3 observer 回调内重入 reload：全部不死锁。
- Q3 subscribe 内 std::function 移动构造重入：全部不死锁。

## Q1 SubscriptionHub

### P01
- 正确性 45 → 43/45：自有存储、drain 锁内 swap+固定 audience、锁外回调、shared_ptr<Subscription>+原子 cancelled、异常隔离、模减过期均正确。取消语义以源码逻辑为准判正确（探针 got b=x 归因时序）。扣 2：drain 并发非互斥、expire_idle 析构重入未单独测试。
- C++质量 20 → 19/20
- 约束遵循 15 → 15/15
- 可维护性 10 → 9/10
- 验证证据 10 → 10/10：自写 8 例边界（含多线程、ASan），ANSWER 诚实标注 TSan 不可用、并发 drain 非互斥风险。
- 小计：96/100
- 硬性扣分触发：否。
- 关键缺陷：无重大；并发 drain 间非串行（题面未强制）。

### P02
- 正确性 45 → 40/45：shared_ptr<Callback>+map、drain 锁外回调、subscription_cutoff 上界冻结新订阅、异常计入、模减过期均正确。实锤缺陷：hub 析构期间 callable 析构链重入 subscribe 时 map UAF/coredump（多线程 coredump，单线程 ASan 复现）。扣 5。
- C++质量 20 → 18/20
- 约束遵循 15 → 15/15
- 可维护性 10 → 9/10
- 验证证据 10 → 10/10
- 小计：92/100
- 硬性扣分触发：否。
- 关键缺陷：hub 析构期 callable 析构重入 subscribe → map UAF/coredump（实测复现）。

### P03
- 正确性 45 → 40/45：shared_ptr<Callback>+vector、drain 锁内 swap+复制、锁外回调、每次投递前 still_active 重查、异常隔离、模减过期均正确。缺陷：(1) drain 每订阅者每事件 any_of 全表重查 O(n²)；(2) TOCTOU（题面允许"至多一次"）；(3) 未自写任何边界测试。
- C++质量 20 → 17/20
- 约束遵循 15 → 15/15
- 可维护性 10 → 7/10
- 验证证据 10 → 7/10：仅公共检查，无自写测试。
- 小计：86/100
- 硬性扣分触发：否。
- 关键缺陷：无自写测试；drain O(n²) 重查。

### P04
- 正确性 45 → 42/45：shared_ptr<Subscription>+原子 alive、drain 锁内 move-out+复制 audience、锁外回调、逐事件查 alive、异常计入、模减过期均正确。取消自身 got=[a] delivered=1 正确；cancel-sibling b= 正确。扣 3：alive 与投递 TOCTOU（可接受）；多线程压测未编写；drain 用 assign 而非 swap。
- C++质量 20 → 19/20
- 约束遵循 15 → 15/15
- 可维护性 10 → 9/10
- 验证证据 10 → 9/10：自写 extra_checks 覆盖广，ASan 跑。
- 小计：94/100
- 硬性扣分触发：否。
- 关键缺陷：无重大；多线程压测缺失（自述）。

### P05
- 正确性 45 → 30/45：自有存储、drain 锁内 swap+复制、removed_ 集合取消、异常计入、模减过期均正确。致命缺陷：drain 在 lock_guard 内 subs_snapshot = subscriptions_，整表拷贝 vector<Subscription>，连带拷贝每个 std::function。探针实测 EXIT=124 死锁。subscribe 也在锁内 push_back(move)（移动，不触发拷贝）。
- C++质量 20 → 14/20：drain 异常分支与正常分支重复；removed_ 集合只增不减。
- 约束遵循 15 → 12/15：违反硬性封顶条件。
- 可维护性 10 → 6/10
- 验证证据 10 → 6/10：仅公共检查，无自写测试。
- 加总：68
- 硬性扣分触发：是（Q1 硬性封顶）。drain 在 mutex 内复制 std::function，实测死锁。总分 ≤ 60。最终 Q1 = 60。
- 关键缺陷：锁内复制 std::function（硬性封顶，实测死锁）。

## Q2 CoalescingCache

### P01
- 正确性 45 → 43/45：单 mutex 包住 entries/index/inflight/flight.done/result（单一同步关系）；owner 锁外 loader+完成时钟、锁内原子发布；invalidate 置 invalidated 不取消 loader；TTL=0/cap=0/空值/失败/异常/递归全 PASS。扣 2：Clock 异常被吞（题面未规定，可接受）；过期判断释放锁查 Clock 后重取锁 continue 重试窗口。
- C++质量 20 → 19/20
- 约束遵循 15 → 15/15
- 可维护性 10 → 9/10
- 验证证据 10 → 10/10：自写 11 例边界 + ASan。
- 小计：96/100
- 硬性扣分触发：否。
- 关键缺陷：无重大。

### P02
- 正确性 45 → 42/45：单 mutex+shared_ptr Flight；thread_local ActiveCall(state*,key) 检测递归；owner 锁外 loader+完成时钟；完成 Clock 异常转 clock_failure（合理降级）。扣 3：entry-clock 异常路径在 catch 内取 state->mutex 后 wait_for_flight；LRU 用 list::remove O(n)。
- C++质量 20 → 18/20
- 约束遵循 15 → 15/15
- 可维护性 10 → 9/10
- 验证证据 10 → 9/10
- 小计：93/100
- 硬性扣分触发：否。
- 关键缺陷：Clock 异常 catch 内加锁 wait（轻微）；LRU O(n)。

### P03
- 正确性 45 → 37/45：双 mutex（state.mtx + flight.mtx）。owner：锁外 loader→flight.mtx 发布→state.mtx 写 ready+erase→notify。waiter：state.mtx 发现 flight→flight.mtx wait done。核心探针全 PASS。实锤缺陷：完成时刻 state_->now()（coalescing_cache.cpp:122）无 try/catch，Clock 抛异常则异常逃逸 get_or_load（违反语义 4），flight 永不置 done → 等待者永久阻塞。探针实测 EXCEPTION ESCAPED。扣 6。
- C++质量 20 → 17/20
- 约束遵循 15 → 15/15
- 可维护性 10 → 8/10
- 验证证据 10 → 5/10：无自写测试。
- 小计：82/100
- 硬性扣分触发：否。
- 关键缺陷：完成 Clock 异常逃逸 API + 等待者永久阻塞；无自写测试。

### P04
- 正确性 45 → 42/45：单 mutex+单 cv+shared_ptr Flight（无 flight 专属 mutex，严格单一同步关系）；thread_local vector<Flight*> 检测递归；owner 锁外 loader+完成时钟（完成时钟包在 try 内，异常转 failed）。扣 3：完成 Clock 异常将成功结果降级为 failed；cache 级 cv 惊群。
- C++质量 20 → 19/20
- 约束遵循 15 → 15/15
- 可维护性 10 → 9/10
- 验证证据 10 → 8/10
- 小计：93/100
- 硬性扣分触发：否。
- 关键缺陷：完成 Clock 异常降级；cache 级 cv 惊群。

### P05
- 正确性 45 → 39/45：双 mutex（state.mtx + flight.done_mtx）。三段式发布。合并/invalidate/TTL/cap/空值/失败/递归/不同 key 探针全 PASS。扣分：(1) ready 新鲜度用 cached_value.empty()||complete_time!=0 哨兵，空值靠 complete_time 兜底（语义脆弱）；(2) ready 过期时不删除旧 entry，stale entry 滞留至下次 invalidate/覆盖；(3) ready_order.remove(key) O(n)；(4) 三段式发布窗口；(5) State 自定义析构显式 clear（多余）。
- C++质量 20 → 16/20
- 约束遵循 15 → 15/15
- 可维护性 10 → 8/10
- 验证证据 10 → 6/10：无自写测试。
- 小计：84/100
- 硬性扣分触发：否。
- 关键缺陷：ready 过期不清理（stale 滞留）；空值新鲜度哨兵脆弱；无自写测试；双 mutex 三段式。

## Q3 RoutingConfig

### P01
- 正确性 45 → 43/45：shared_ptr<const Config> published_ 原子发布；valid 全量校验失败即 false 且不变；版本+1；current() 用 thread_local unordered_map<this, shared_ptr> per-instance pin；reload 锁内拷 shared_ptr observer 列表、锁外通知；subscribe 锁外 make_shared、unsubscribe 锁外 reset；异常隔离+error count；find_target 单快照最长前缀。扣 2：TLS pin 实例析构后 dangling key（超题面）。
- C++质量 20 → 19/20
- 约束遵循 15 → 15/15
- 可维护性 10 → 9/10
- 验证证据 10 → 10/10：自写 11 例 + ASan + clang 交叉。
- 小计：96/100
- 硬性扣分触发：否。
- 关键缺陷：TLS pin dangling key（理论，超题面）。

### P02
- 正确性 45 → 43/45：与 P01 等价设计（shared_ptr<const Config> current_ + 文件作用域 thread_local unordered_map<this,...> pin）。扣 2：TLS dangling key；valid 锁外调用（纯函数，正确）。
- C++质量 20 → 18/20
- 约束遵循 15 → 15/15
- 可维护性 10 → 9/10
- 验证证据 10 → 9/10：自写 edge + ASan + clang。
- 小计：94/100
- 硬性扣分触发：否。
- 关键缺陷：TLS dangling key（理论）。

### P03
- 正确性 45 → 42/45：shared_ptr<const Config> current_snapshot_ + 文件作用域 TLS pin；校验失败不变；版本+1；通知锁外；异常隔离；find_target 最长前缀。扣 3：subscribe 在锁内 make_shared<Observer>(std::move(observer))（routing_config.cpp:99-102）——std::function 移动构造不触发用户代码（探针不死锁），但字面违反"移动锁外"。
- C++质量 20 → 18/20
- 约束遵循 15 → 13/15：subscribe 锁内执行 Observer 移动构造（字面违反），扣 2。
- 可维护性 10 → 8/10
- 验证证据 10 → 8/10：无自写测试。
- 小计：89/100
- 硬性扣分触发：否。
- 关键缺陷：subscribe 锁内移动构造 Observer（字面违规，实际不触发用户代码）；无自写测试。

### P04
- 正确性 45 → 43/45：shared_ptr<const Config> current_ + 文件作用域 TLS pin；校验失败不变；版本+1；notify_observers 独立函数锁外通知；subscribe 锁外 make_shared、unsubscribe 锁外 reset；异常隔离；find_target 最长前缀。扣 2：TLS dangling key。
- C++质量 20 → 19/20
- 约束遵循 15 → 15/15
- 可维护性 10 → 9/10
- 验证证据 10 → 8/10：自写 extra_checks + g++/clang++ 交叉。
- 小计：94/100
- 硬性扣分触发：否。
- 关键缺陷：TLS dangling key（理论）。

### P05
- 正确性 45 → 42/45：shared_ptr<const Config> current_ + 函数内 thread_local unordered_map<const void*, shared_ptr> pin；校验失败不变；版本+1；reload 锁内 observers_snapshot = observers_（拷 shared_ptr）锁外通知；异常隔离；find_target 最长前缀。扣 3：(1) subscribe 锁内 make_shared<Observer>(move)（同 P03 字面违规）；(2) current() 持 mutex_ 时写 TLS map（过度持锁）；(3) 无自写测试。
- C++质量 20 → 18/20
- 约束遵循 15 → 13/15：subscribe 锁内移动构造 Observer，扣 2。
- 可维护性 10 → 8/10
- 验证证据 10 → 8/10：无自写测试。
- 小计：89/100
- 硬性扣分触发：否。
- 关键缺陷：subscribe 锁内移动构造 Observer（字面违规）；current() 持锁写 TLS（过度持锁）；无自写测试。

## 关键差异观察（基于匿名 ID，不外推身份）

1. callable 锁边界（Q1 硬性封顶）：P01/P04 用 shared_ptr<Subscription>+原子标志；P02/P03 用 shared_ptr<Callback>+map/vector；这四者 drain 均只复制 shared_ptr 控制块，合规。P05 唯一在锁内复制整个 vector<Subscription>（含 std::function），实测死锁，触发 Q1 硬性封顶（≤60）。本轮最显著的正确性分水岭。
2. 析构重入健壮性（Q1）：P02 在 hub 析构期 callable 析构链重入 subscribe 时 coredump（map UAF）；P05 析构链重入 subscribe 时 UAF（ASan）；P01/P03/P04 仅 LSan leak（无害）。
3. Clock 异常隔离（Q2 语义 4）：P01/P05 吞掉返回 ok（不缓存）；P02/P04 降级为 failed；P03 唯一让 Clock 异常逃逸 get_or_load 且 flight 永不 done（等待者永久阻塞）。
4. 单锁 vs 双锁（Q2 语义 7）：P01/P04 严格单 mutex 覆盖 done/result/discovery；P03 用 flight 专属 mutex（行为等价）；P05 用三段式发布（行为正确但窗口多）。均接受。
5. Q3 subscribe 锁内 make_shared 移动：P01/P02/P04 锁外构造；P03/P05 锁内 make_shared<Observer>(move)——std::function 移动不触发用户代码（探针不死锁），字面违反"移动锁外"但不触发硬性封顶。
6. 自写测试投入：P01（8+11+11 例，含 ASan/clang）与 P04（extra + ASan/clang）验证最充分；P02 有 edge 用例；P03/P05 全程无自写测试，其隐藏缺陷（P03 Clock 异常逃逸、P05 锁内拷贝）均未被自身发现。
7. run_public_checks.sh 完整性：所有 Pxx 均未削弱检查。

## 评分不确定 / 未验证项

- TSan 全员未跑（环境缺 libtsan_preinit.o）。所有"无数据竞争"结论基于锁覆盖 + shared_ptr 原子引用计数 + ASan 内存安全，非 TSan 实证。对 P03/P05 的双 mutex 设计尤其属未验证风险。
- P01 Q1 cancel-sibling 探针结果（got b=x）与源码逻辑表面相悖；倾向探针 b_id 订阅时序问题，以源码事实为准判 P01 取消语义正确，未据此扣分；若验收方复现 P01 确实投递了被取消的 sibling，P01 Q1 正确性应再扣 2–3。
- P02 Q1 coredump 与 P05 Q1 死锁/UAF 均为探针实测复现。
- P05 Q2 ready 过期不清理：仅静态分析，未构造确定性探针；按"可判定优先"以源码事实扣分。
- P03/P05 Q3 subscribe 锁内移动：判定为"字面违反但不触发用户代码、不死锁"，未触硬性封顶。
