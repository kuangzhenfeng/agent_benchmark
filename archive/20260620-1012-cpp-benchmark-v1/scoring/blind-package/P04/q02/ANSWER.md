# 作答说明

## 修改摘要

完整实现 `CoalescingCache`（替换 `src/coalescing_cache.cpp` 中的 `State` 与所有方法）。公开 API、`LoadResult`、`LoadStatus` 不变。

### 状态机与数据结构（私有，在 .cpp 内）

- `State` 持有：`capacity`、`now`（Clock）、`mutex`、`condition_variable cv`、LRU 容器（`std::list<pair<string,Entry>>` front=MRU + `unordered_map<string,list-iter>`）、在途表 `unordered_map<string, shared_ptr<Flight>>`。
- `Flight`（匿名命名空间）：`result`、`done`、`invalidated`。**不含自己的 mutex**——这些字段只在 `State::mutex` 下读写（语义 7：所有发现该 flight 的调用者经同一同步关系观察完整结果）。
- `Entry`：`value`、`expire_tick`（= loader 完成时钟 + ttl）。

### 锁边界（语义 2）

- `Clock`（`now()`）与 `loader` 一律在 `State::mutex` **之外**调用：入口处取 `now_entry`（未持锁），owner 在 loader 返回后取 `now_done`（已 `unlock`）。因此 Clock 内重入 `invalidate` 或请求**不同** key 都能正常获取 `mutex`，不死锁。
- 递归同 key 检测：匿名命名空间 `thread_local std::vector<Flight*> active_loads`。owner 运行 loader 前把 flight 入栈、发布后出栈；任何线程在 `flights_` 里找到 flight 时先 `is_active_load` 判断——命中即返回 `recursive_load`，不等待自己。

### get_or_load 流程

1. 锁外取 `now_entry`。
2. 加锁：若 `ttl>0` 且 ready 命中且 `now_entry < expire_tick` → 刷新 LRU、返回 ok 拷贝；过期则剔除并继续。
3. 查 `flights_`：命中 → 若本线程已持有该 flight（递归）返回 `recursive_load`；否则拷 `shared_ptr<Flight>` 保活，`cv.wait` 到 `done`，在锁内拷贝 result 返回。
4. 否则成为 owner：新建 flight 入 `flights_`、压 `active_loads`、解锁。
5. 锁外 `try { result = loader(key); now_done = now(); } catch(...) { result=failed; }`——异常不跨 API（语义 4）。
6. 加锁发布（**单一原子转换**）：写 `result`、置 `done`；`cacheable = ok && ttl>0 && !invalidated && capacity>0` 决定是否写 ready（命中则更新值+刷新 LRU，否则前插并按容量淘汰尾部）；从 `flights_` 移除（flight 变为不可发现）；`active_loads.pop_back()`；`cv.notify_all()`；返回 result。

### invalidate 竞态（语义 5）

- 锁内：从 ready 删除该 key；若该 key 在途，置 `flight->invalidated=true`。**不取消 loader**；owner 发布时因 `invalidated` 跳过 ready 回写，但 result 仍返回给已加入的等待者；新调用在 flight 仍可发现期间可继续加入，flight 不可发现后看到 ready 或开新 flight。

### LRU / 容量（语义 6）

- 仅淘汰 `ready`；`flights_` 与 ready 分离，永不被 LRU 淘汰，故「全部在途」亦安全。
- 访问 ready 值 `splice` 到 front 刷新 LRU。
- `capacity==0`：`cacheable` 要求 `capacity>0`，永不缓存；并发仍经 flight 合并。

### 单次完成语义（语义 7）

owner 在 `State::mutex` 的一次临界区内完成「定 result、置 done、定 ready 写入（含 invalidate 结论）、移除 flight、notify」，因此 flight 变不可发现前一切已就绪；等待者持 `shared_ptr` 并经同一 `mutex`+`cv` 观察完整结果，无第二把 mutex 读写 done/result。

## 验证

- `./run_public_checks.sh`：`public_checks.cpp`（6 线程合并单次 loader）、`clock_reentrancy_checks.cpp`（Clock 内 invalidate）、新增 `extra_checks.cpp`，全部通过（g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread）。
- `extra_checks.cpp` 覆盖：TTL 到期重载、TTL=0 不缓存但合并、loader 失败共享且不缓存、loader 异常被捕获、递归同 key 返回 `recursive_load`、loader 内请求不同 key 不死锁、加载在途时 invalidate 阻止回写、LRU 淘汰与访问刷新、capacity=0 永不缓存、invalidate 立即移除 ready、32 线程压力下单次 loader。
- ASAN（`-fsanitize=address`，`detect_leaks=0`，`halt_on_error=1`）单独跑 `extra_checks.cpp` 通过，无数据竞争报告（ASAN 无 TSan 能力，但验证了内存安全）。
- 说明：本机 ASAN `detect_leaks=1` 对最简用例也产生 `DEADLYSIGNAL` 级联（环境 LSan 问题），故未启用 leak 检测。

## 剩余风险

- 未使用 ThreadSanitizer 环境定量验证「无数据竞争」；逻辑上 result/done/invalidated 均在 `State::mutex` 下、LRU/ready/flights 亦在锁下，应无竞争，但缺 TSan 佐证。
- Clock 在 loader 成功后第二次调用；若该 Clock 抛异常，结果按失败处理（不缓存），与「成功结果以完成时钟为 TTL 起点」存在边界（极罕见，clock 抛异常时无法计时）。
