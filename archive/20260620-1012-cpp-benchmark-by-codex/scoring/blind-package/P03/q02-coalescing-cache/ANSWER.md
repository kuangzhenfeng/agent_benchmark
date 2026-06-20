# 作答说明 — Q2 CoalescingCache

## 修改摘要

starter 是空壳（`get_or_load` 恒返回 `failed`，`invalidate`/`size` 空实现）。完整重写了 `State` 与 `get_or_load`，保持公共 API、`LoadStatus`、`LoadResult` 不变。

### 状态机

每个 key 在给定时刻处于三种状态之一：**ready（LRU 缓存命中）**、**in-flight（有 owner 正在跑 loader）**、**absent**。

- `State::entries`（`std::list<Entry>`，front=MRU）+ `index`（`unordered_map<key, list::iterator>`）：ready 缓存 + LRU 顺序。访问即 `splice` 到 front 刷新 LRU 位置；满容量淘汰 `back`（LRU）。
- `State::inflight`（`unordered_map<key, shared_ptr<Flight>>`）：在途加载。`Flight` 含 `owner`（`std::thread::id`）、`cv`、`done`、`invalidated`、`result`。

### 在途状态的所有者

- **owner 判定**：`get_or_load` 进锁后，若 `inflight[key]` 存在且 `flight->owner == this_thread::get_id()` → 递归同 key，立即返回 `recursive_load`（不等待自己）。否则：
  - 已有 flight（不同线程）→ **coalesce（等待者）**：`cv.wait` 直到 `done`，在锁内读 `flight->result`。
  - 无 flight，ready 未过期 → 直接返回缓存值。
  - 无 flight，ready 过期或 absent → **成为 owner**：新建 `Flight`（`owner=self`）插入 `inflight`，**释放锁**，在锁外跑 `loader`。
- **单一同步点**（满足第 7 条）：owner 完成 loader 后，**在同一把 `State::mutex` 下**依次完成：①决定最终 `LoadResult`；②决定是否写 ready cache（含 `invalidate` 的结果——若 `flight->invalidated` 则不写回）；③`flight->result = ...; flight->done = true;`（安全发布给等待者）；④`inflight.erase(key)`（flight 不再可发现）。然后才解锁、`cv.notify_all()`。任何找到该 flight 的调用者都经由这同一同步关系观察到完整、一致的结果；不另开 mutex 读写 `done`/`result`。
- **flight 不可发现后**的新调用者：要么命中 ready（若 owner 写回了），要么（结果不可缓存 / 已 invalidate）开启新 flight。

### loader / Clock 的锁边界（第 2 条）

- `loader(key)` 仅在 owner 段调用，**全程在 `State::mutex` 之外**。loader 重入请求**不同** key 安全（锁外）；重入请求**同** key 走上面「owner 判定」返回 `recursive_load`。
- `Clock`（`s.now()`）两次调用均在锁外：①判断 ready 是否过期前；②loader 成功后取 TTL 起点。`clock_reentrancy_checks` 的 Clock 在内部 `invalidate("unrelated")`，因 Clock 不持锁、`invalidate` 只短暂取 `State::mutex`，不会死锁。

### TTL 起点（第 3 条）

成功结果以 **loader 完成时**的时钟作为 TTL 起点（`loaded_at = s.now()`）。命中判断用模减 `(now - loaded_at) >= ttl`。`ttl == 0` 不写 ready，但并发调用仍 coalesce 同一次在途加载（所有等待者都 `cv.wait` 同一个 flight）。`capacity == 0` 永不保留 ready。

### loader 失败 / 异常（第 4 条）

- `loader` 返回 `loader_failed` → 不写 ready，所有等待者经同一 flight 拿到失败，后续调用可重载。
- `loader` 抛异常 → `try/catch(...)` 捕获，转成 `LoadResult::failed("loader threw")`，**不跨 API 边界**抛出；同样不写 ready、通知等待者。

### invalidate 竞态（第 5 条）

`invalidate(key)` 进锁：移除 ready 中的 key；若该 key 正在 `inflight`，置 `flight->invalidated = true`（**不取消 loader**）。owner 完成时检查 `invalidated`：若已置位，即便成功也**不写回 ready cache**，但 `result` 照常发布给已登记的等待者（它们仍拿到这次结果）。invalidate 之后再来的调用可加入新 flight。在 invalidate 之前已登记的等待者继续等待同一 flight（题目允许「之后的调用可继续加入这次在途加载」）。

### LRU（第 6 条）

容量由构造函数给定。ready 命中 `splice` 到 front；插入新条目满则淘汰 `back`。在途 flight 不进 `entries`，故不会因「全在途」而淘汰 ready（也不违反内存安全——淘汰只动 `entries`/`index`，flight 由 `shared_ptr` 独立保活）。`capacity == 0` 时所有插入路径被 `cacheable` 条件跳过。

## 验证

```bash
cd q02-coalescing-cache
./run_public_checks.sh                       # public + clock_reentrancy  → EXIT 0

g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread \
    -I include src/coalescing_cache.cpp tests/extra_boundary_checks.cpp -o /tmp/q2_extra && /tmp/q2_extra
# → ALL PASS（11 个用例）

g++ ... -fsanitize=address -fno-omit-frame-pointer ... extra_boundary_checks.cpp && /tmp/q2_asan
# → ALL PASS，无内存错误/泄漏
```

新增 `tests/extra_boundary_checks.cpp` 覆盖：
1. 并发 miss 合并、loader 仅 1 次、结果一致；
2. TTL 过期重载 + TTL 锚点=完成时刻；
3. `ttl=0` 不缓存但并发仍合并；
4. loader 失败不进 ready、可重载；
5. loader 抛异常不跨 API 边界；
6. 同 key 递归返回 `recursive_load`；
7. invalidate 与在途 loader 竞态（不取消 loader、不写回 ready、后续可重载）；
8. LRU 容量淘汰 + 访问刷新位置；
9. `capacity=0` 不缓存但合并；
10. `size()` 自包含、仅统计 ready；
11. 多线程压力（混合 key + invalidate）不死锁、`size <= capacity`。

## 剩余风险

- **TSan 未跑**（本机 toolchain 缺 `libtsan_preinit.o`）。所有共享状态（`entries`/`index`/`inflight`/`Flight::done`/`result`）统一由 `State::mutex` 保护；`Flight::result` 仅在持锁时读写；`shared_ptr` 控制块原子引用计数。理论上无竞争，ASan 通过。
- **等待者唤醒顺序**：`notify_all()` 一次性唤醒所有 coalesced 等待者，它们竞争性重取锁读 `result`；题目未要求特定唤醒顺序（如 FIFO），故未实现公平队列。如需 FIFO 需引入 ticket，超出题目要求。
- **Clock 抛异常**：当前 swallow 为「按未过期 / `loaded_at=0`」处理。题目未规定 Clock 异常语义，属实现选择。
- **过期判断窗口**：释放锁查 Clock 后、再取锁写回期间，可能 loader 已被另一调用刷新；用 `loaded_at` 比对 + `continue` 重试保证一致性，但极端并发下存在一次额外循环（非正确性问题）。
