# 评分员 B 独立报告（benchmark-v1 盲评）

本报告由独立第二评分员完成。所有评分依据仅为允许读取的 `blind-package/` 与 `scorer-reference/` 目录内容（源码 + 公开检查实测 + 参考验收矩阵）。**全程未读取任何身份相关文件（mapping.private.md、participants.md、agents/、submissions/、questions/），未联网，未尝试推断 P01..P06 对应的 Agent 或模型。**

测试环境：g++ 9.4.0，C++17，每个 `run_public_checks.sh` 自带 `timeout 5s`（退出码 124 = 超时/死锁）。ASAN 用 `-fsanitize=address,undefined -g`，单独重编各测试以定位失败性质。

## 评分汇总（300 满分）

| ID | Q1 | Q2 | Q3 | 总分 | 排名 |
|----|-----|-----|-----|------|------|
| P06 | 93 | 92 | 90 | **275** | 1 |
| P01 | 92 | 92 | 88 | **272** | 2 |
| P02 | 93 | 88 | 88 | **269** | 3 |
| P04 | 56 | 81 | 77 | **214** | 4 |
| P05 | 56 | 88 | 55 | **199** | 5 |
| P03 | 56 | 54 | 55 | **165** | 6 |

排名（仅用 P01..P06，不猜测身份）：**P06 > P01 > P02 > P04 > P05 > P03**

---

## P01

### Q1 subscription-hub: 正确性 42/45 | C++质量 18/20 | 约束 14/15 | 可维护 9/10 | 验证 9/10 | 总分 92
- **公开检查**：pass 3/3，3 次稳定通过（非 ASAN + ASAN 均 0）。
- **源码事实**：
  - 私有 `Subscription` 以 `std::string` 持有 topic、以 `shared_ptr<Callback>` 持有回调；`unsubscribe`/`expire_idle` 把待销毁 callback 移到局部 vector，**解锁后**释放（最后所有者析构在锁外）。
  - `drain` 在锁内一次性 `std::move` 截取整条 `pending_` 并清空原队列；回调与 tick 更新均在锁外执行。
  - 异常用 `try/catch` 隔离，`++delivered` 在每次尝试后执行，事件已从队列移除不会重放。
  - TTL 使用 `static_cast<uint32_t>(now_tick - last_active_tick) >= ttl` 模减，wrap-around 正确。
  - 轻微瑕疵：`drain` 内更新 `last_active_tick` 时又对每个回调加一次锁做线性查找，锁粒度偏细碎（不影响正确性）。
- **ANSWER.md**：填写了修改说明、所有权/drain 冻结/异常隔离/模减设计与验证方式。
- **依据**：正确处理 callable 生命周期、drain 批次冻结、异常不重放、TTL 模减；通过公开检查。

### Q2 coalescing-cache: 正确性 42/45 | C++质量 18/20 | 约束 14/15 | 可维护 9/10 | 验证 9/10 | 总分 92
- **公开检查**：pass 3/3，3 次稳定通过。
- **源码事实**：
  - `thread_local loading_keys`（含 cache 实例 + key）实现递归加载识别，重入立即返回 `recursive_load`。
  - Clock 在锁外调用（先取 now 再加锁查 ready）；Flight 含 `mtx/cv/done/result/should_cache`。
  - owner 锁外调用 loader、捕获异常转 `loader_failed`；完成后在**同一锁内**写结果 + 完成标志 + ready 决定，再 `notify_all`。
  - invalidate 仅删 ready entry 并对在途 flight 置 `should_cache=false`，不取消 loader。
  - 容量淘汰仅作用于 ready entry；TTL 0 不写 ready。
- **ANSWER.md**：说明了 flight 合并、完成+缓存原子决定、invalidate 不缓存、递归加载检测。
- **依据**：状态机完整，通过并发合并、clock 重入、递归加载等检查。

### Q3 routing-config: 正确性 40/45 | C++质量 18/20 | 约束 14/15 | 可维护 8/10 | 验证 8/10 | 总分 88
- **公开检查**：`run_public_checks.sh` 脚本整体 1/3（受 ASAN cross_instance 间歇性超时拖累）；**单独非 ASAN 跑 cross_instance 多次稳定 0（通过）**；单独 ASAN cross_instance 偶发超时（约 30%，属 ASAN 开销 + 5s 超时，无确定性 ASAN 堆错误报告）。
- **源码事实**：
  - **实例成员** `mutable std::map<std::thread::id, std::shared_ptr<const Config>> pinned_`（per-instance，正确）；`current()` 锁内 `pinned_[tid] = published_` 后返回 `*pinned_[tid]`，引用被固定。
  - reload 在短锁内取得 `shared_ptr<Observer>` 副本 + 已发布 snapshot，**完全释放锁**后逐个通知；异常隔离计数。
  - subscribe 先封装 `shared_ptr<Observer>`，unsubscribe 延迟销毁。
  - 失败校验不发布/不递增版本/不通知；最长前缀匹配正确。
- **ANSWER.md**：说明快照发布、per-instance TLS pin、锁外通知。
- **依据**：设计正确（per-instance pin + 锁外通知）；ASAN cross 间歇超时判为开销非真实 bug，扣少量验证证据分。

**P01 总分：272/300**

---

## P02

### Q1 subscription-hub: 正确性 43/45 | C++质量 18/20 | 约束 14/15 | 可维护 9/10 | 验证 9/10 | 总分 93
- **公开检查**：pass 3/3，3 次稳定通过。
- **源码事实**（见 `src/subscription_hub.cpp`）：
  - `subscribe` 锁外把 callback 封装为 `shared_ptr<Callback>`；私有 `Subscription` 存该指针。
  - `unsubscribe`（行 17-28）锁内 `std::move(it->callback)` 到局部后 erase，**最后所有者析构在锁外**。
  - `drain`（行 38-87）锁内一次性 `std::move` 截取 `pending_` 并清空；记 `max_original_id` 冻结本批次订阅集合，回调 publish 的新事件不进本批次，新订阅不补入。
  - 回调与 tick 更新在锁外；异常 `catch(...)` 隔离，`++delivered` 每次尝试后执行，事件已移除不会重放。
  - `expire_idle`（行 89-105）模减 `static_cast<uint32_t>(now - last) >= ttl`，待销毁 callback 移到局部、解锁后释放。
- **ANSWER.md**：填写完整。
- **依据**：所有权/drain 冻结/异常/TTL 全部正确；通过公开检查。

### Q2 coalescing-cache: 正确性 40/45 | C++质量 16/20 | 约束 14/15 | 可维护 9/10 | 验证 9/10 | 总分 88
- **公开检查**：pass 3/3，3 次稳定通过。
- **源码事实**：单 mutex + condition_variable；递归加载靠 `flight->owner_thread == this_thread::get_id()` 检测；完成 + 缓存决定在同一锁内原子完成；invalidate 对在途 flight 设不缓存标志。
- **扣分点**：递归加载检测用 `owner_thread` 比对，能覆盖同线程 owner 重入，但相对 thread-local 集合方式稍弱（理论上对"非 owner 但同线程 waiter 重入"覆盖度依赖实现细节）；Clock 在锁外（正确）。
- **依据**：通过合并/clock 重入/TTL/invalidate 检查；递归加载处理可用但非最稳健。

### Q3 routing-config: 正确性 40/45 | C++质量 18/20 | 约束 14/15 | 可维护 8/10 | 验证 8/10 | 总分 88
- **公开检查**：脚本 1/3（ASAN cross 间歇超时）；非 ASAN cross 多次稳定通过。
- **源码事实**（见 `src/routing_config.cpp`）：
  - **per-instance TLS**：`thread_local std::unordered_map<const RoutingConfig*, shared_ptr<const Config>> pins; auto& pin = pins[this]`（行 13-14，正确）；按 version 比较，过期才刷新 pin。
  - `snapshot()` 锁内返回 shared_ptr 副本。
  - reload（行 45-72）锁内校验已过 → 设 version+1 → 构造新 immutable snapshot → 发布 → 收集 `shared_ptr<Observer>` 副本，**锁外**逐个通知，异常锁内计数。
  - subscribe/unsubscribe 用 `shared_ptr<Observer>`，unsubscribe 延迟销毁。
  - 最长前缀匹配正确（行 94-108）。
- **ANSWER.md**：填写完整。
- **依据**：per-instance pin + 锁外通知设计正确；ASAN cross 间歇超时判为开销。

**P02 总分：269/300**

---

## P03

### Q1 subscription-hub: 正确性 24/45 | C++质量 10/20 | 约束 13/15 | 可维护 6/10 | 验证 3/10 | 总分 56（硬性封顶 ≤60）
- **公开检查**：`callable_lifetime_checks` **失败 3/3（124 超时=死锁）**，3 次确定性失败。
- **源码事实**：`unsubscribe` 与 `expire_idle` 在**持有 mutex** 时使用 `std::remove_if`/`erase`，被擦除元素中的 callable 在锁内销毁；当该 callable 是最后一个所有者时，其析构函数重入 hub API → 死锁。
- **硬性封顶**：题面明确"任何 `Callback` 的复制/移动/销毁都不能在 mutex 下运行"，违反硬性封顶条件 → 总分 ≤ 60。
- **依据**：确定性死锁，正确性大幅扣分并触发硬性封顶。

### Q2 coalescing-cache: 正确性 22/45 | C++质量 11/20 | 约束 13/15 | 可维护 6/10 | 验证 2/10 | 总分 54
- **公开检查**：`clock_reentrancy_checks` **失败 3/3（124 死锁）**，3 次确定性失败；其余检查通过。
- **源码事实**：在 cache mutex 内调用 `Clock`（`state_->now()` 在持锁的缓存查找路径中）→ Clock 内部调用 `invalidate` 重入同一 mutex → 死锁。源码注释提到应支持递归加载，但实际无递归加载检测代码。
- **依据**：clock 重入死锁 + 缺递归加载检测；正确性大幅扣分。

### Q3 routing-config: 正确性 22/45 | C++质量 12/20 | 约束 13/15 | 可维护 6/10 | 验证 2/10 | 总分 55
- **公开检查**：`observer_copy_reentrancy_checks` **失败 3/3（124 死锁）**，3 次确定性失败；pub 单独通过。
- **源码事实**：per-instance TLS pin（`thread_local std::map<const RoutingConfig*, ...>`）方向正确；但 reload 在**锁内** `for (const auto& observer : observers_) observers.push_back(observer.second)` 复制 Observer（std::function 复制构造运行用户代码）→ observer 复制重入死锁。
- **依据**：pin 设计对，但观察者复制在锁内导致确定性死锁，正确性大幅扣分。

**P03 总分：165/300**

---

## P04

### Q1 subscription-hub: 正确性 24/45 | C++质量 11/20 | 约束 13/15 | 可维护 6/10 | 验证 2/10 | 总分 56（硬性封顶 ≤60）
- **公开检查**：`callable_lifetime_checks` **失败 3/3（124 死锁）**，3 次确定性失败。
- **源码事实**：与 P03 同型——`unsubscribe`/`expire_idle` 在锁内 `remove_if`/`erase` 销毁 callable，最后一个所有者析构重入死锁。
- **硬性封顶**：违反 callable 销毁不得在锁内的硬性条件 → 总分 ≤ 60。

### Q2 coalescing-cache: 正确性 37/45 | C++质量 16/20 | 约束 14/15 | 可维护 8/10 | 验证 6/10 | 总分 81
- **公开检查**：pass 3/3，3 次稳定通过。
- **源码事实**：`shared_mutex`；Flight 带 `wait_flags`（基于标志的等待）；Clock 在锁外调用。
- **扣分点**：**未实现递归加载检测**（同线程同 key owner 重入会等待自己 → 死锁风险）；验收矩阵中"loader 请求同 key 立即 recursive_load"未覆盖。基础合并/TTL/invalidate 正常。
- **依据**：通过基础并发检查，但递归加载维度缺失，正确性扣分。

### Q3 routing-config: 正确性 33/45 | C++质量 15/20 | 约束 14/15 | 可维护 8/10 | 验证 7/10 | 总分 77
- **公开检查**：pub 通过；非 ASAN cross 多次稳定通过；ASAN cross 间歇超时（约 20%）；obs 通过。
- **源码事实**：`shared_mutex` + `state_ shared_ptr<State>` + `history_ list`。**生命周期缺陷**：`current()` 返回 `*state_->current` 但**无 TLS pin**——读锁释放后，另一线程 reload 减少 state_ 引用计数，返回的 `const Config&` 可能悬空。
- **依据**：基础快照发布/最长前缀正确，但 `current()` 兼容 API 生命周期未正确固定（验收矩阵"reload 后旧 current 引用仍可安全读取"+"跨两实例 current()"未满足），正确性扣分；未触发硬性封顶（未在锁内销毁 function，但设计缺陷导致扣分）。

**P04 总分：214/300**

---

## P05

### Q1 subscription-hub: 正确性 24/45 | C++质量 11/20 | 约束 13/15 | 可维护 6/10 | 验证 2/10 | 总分 56（硬性封顶 ≤60）
- **公开检查**：`callable_lifetime_checks` **失败 3/3（124 死锁）**，3 次确定性失败。
- **源码事实**：`drain` 在锁内拷贝构造 `snapshot.subscribers`（拷贝 `Observer`/`std::function` 可能在锁内运行用户代码）；设 `in_drain_` 标志。callable 复制/销毁路径在 mutex 内 → 重入死锁。
- **硬性封顶**：违反硬性条件 → 总分 ≤ 60。

### Q2 coalescing-cache: 正确性 40/45 | C++质量 17/20 | 约束 14/15 | 可维护 9/10 | 验证 8/10 | 总分 88
- **公开检查**：pass 3/3，3 次稳定通过。
- **源码事实**：`loading_keys_` 按 tid（受锁保护）做递归加载检测；Clock 在锁外；invalidate 设标志；完成+缓存决定原子。
- **依据**：状态机完整，通过并发合并/clock 重入/递归加载检查。

### Q3 routing-config: 正确性 22/45 | C++质量 12/20 | 约束 13/15 | 可维护 6/10 | 验证 2/10 | 总分 55
- **公开检查**：`observer_copy_reentrancy_checks` **失败 3/3（124 死锁）**，3 次确定性失败；pub 通过；ASAN cross 间歇超时。
- **源码事实**：`history_ vector<shared_ptr<const Config>>` + `published_ptr_`；reload **锁内复制观察者按值**通知 → observer 复制重入死锁。current() 在锁内返回 `*published_ptr_` 且无 per-instance pin（history_ 不收缩使 Config 暂存，但跨线程引用未正确固定，属隐患）。
- **依据**：观察者复制锁内导致确定性死锁，正确性大幅扣分。

**P05 总分：199/300**

---

## P06

### Q1 subscription-hub: 正确性 43/45 | C++质量 18/20 | 约束 14/15 | 可维护 9/10 | 验证 9/10 | 总分 93
- **公开检查**：pass 3/3，3 次稳定通过。
- **源码事实**：`shared_ptr<Callback>` + initial_ids set；unsubscribe 延迟销毁（锁外释放最后所有者）；drain 截取队列并冻结批次；异常隔离 + 不重放；TTL 模减。
- **依据**：所有权/drain 冻结/异常/TTL 全部正确，通过公开检查。

### Q2 coalescing-cache: 正确性 42/45 | C++质量 18/20 | 约束 14/15 | 可维护 9/10 | 验证 9/10 | 总分 92
- **公开检查**：pass 3/3，3 次稳定通过。
- **源码事实**：`thread_local std::unordered_set active_loads` 递归加载检测；Clock 锁外；完成+缓存决定在同一锁内原子；invalidate 延迟获取 flight 锁避免竞争。
- **依据**：状态机完整，通过全部检查。

### Q3 routing-config: 正确性 41/45 | C++质量 18/20 | 约束 14/15 | 可维护 9/10 | 验证 8/10 | 总分 90
- **公开检查**：脚本 1/3（ASAN cross 间歇超时）；**非 ASAN cross 多次稳定通过**；ASAN cross 间歇超时（约 20%，开销）；obs 通过 3/3。
- **源码事实**：`thread_pins_ unordered_map<thread::id, shared_ptr<const Config>>`（per-instance）在独立 `pin_mutex_` 下；current() 设置 `thread_pins_[tid]` 固定引用；reload 用 `shared_ptr<Observer>`、锁外通知；失败不发布；最长前缀正确。
- **依据**：per-instance pin + 锁外通知设计正确；obs 检查稳定通过；ASAN cross 间歇超时判为开销非真实 bug。

**P06 总分：275/300**

---

## 关键判定说明

1. **确定性失败（死锁）判为真实缺陷**：P03/P04/P05 的 Q1 callable 销毁在锁内、P03 的 Q2 clock 在锁内、P03/P05 的 Q3 observer 复制在锁内——均 3/3 次确定性 124 超时，且源码可定位死锁路径，按硬性封顶或正确性大幅扣分。
2. **Q3 ASAN cross_instance 间歇超时判为开销**：P01/P02/P04/P06 的非 ASAN cross 多次稳定通过，源码设计正确（per-instance TLS pin + 锁外通知）；ASAN 下偶发超时无确定性 ASAN 堆错误报告，归因于 ASAN 开销 + 5s 超时，仅扣少量验证证据分，不判为真实并发 bug。P04 例外：其 `current()` 无 TLS pin 存在真实生命周期隐患，单独扣分。
3. **递归加载检测**：P02（owner_thread 比对）、P04（缺失）相对 thread-local 集合方案（P01/P05/P06）偏弱，相应扣分。
4. 公开检查通过不代表满分；已结合源码逐条对照验收矩阵判定组合缺陷维度的真实处理情况。

评分员 B，未读取任何身份信息
