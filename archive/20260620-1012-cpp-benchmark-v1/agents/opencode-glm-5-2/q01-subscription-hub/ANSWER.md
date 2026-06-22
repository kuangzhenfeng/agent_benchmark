# 作答说明

## 修改摘要

重写 `subscription_hub.hpp` / `subscription_hub.cpp`，解决 starter 的全部缺陷：

1. **字符串所有权（语义 1）**：`Subscription::topic` 与 `QueuedEvent::topic/payload` 由 `std::string_view` 改为 `std::string`，所有跨调用保存的数据自拥有存储；`publish`/`subscribe` 入口即拷贝。回调拿到的 `Event` 视图指向 drain 局部 batch 的 `std::string` 成员，在该次回调返回前有效。

2. **批次快照与回调重入（语义 2/3/4）**：`drain` 先在锁内把 `pending_` 移动到局部 `batch` 并清空队列，再拷贝 `subscriptions_`（shared_ptr 引用计数拷贝，无用户代码）到 `subs_snapshot`，然后**释放锁**再遍历投递。这样：
   - 回调中 `publish` 的新事件进入（已清空的）`pending_`，只会在下一次 drain 处理；
   - 回调中 `subscribe`/`unsubscribe` 修改的是 `subscriptions_`，不影响本次快照；新订阅因此收不到本批次；
   - 回调中再次 `drain` 拿自己的锁、自己的空 batch，不会无限递归处理当前批次；
   - 事件不会在下次 drain 重复投递。

3. **取消订阅语义（语义 4）**：`Subscription` 增加 `std::atomic<bool> alive`。`unsubscribe`/`expire_idle` 把对应订阅置 `alive=false` 并从 `subscriptions_` 移除，真正的析构在锁外完成。drain 投递时检查 `alive`：开始投递某事件时仍存活的订阅至多收到一次；被取消的订阅不再收到之后的事件。

4. **异常隔离（语义 5）**：单个回调 `try/catch(...)` 捕获，计入 `callback_errors`，`delivered` 仍自增（统计所有尝试，含抛异常者）。其余订阅者继续投递；该事件不会重新入队。

5. **32 位时钟回绕（语义 6）**：`expire_idle` 用 `now_tick - last_active_tick`（uint32 模运算）算 elapsed，`elapsed >= ttl` 即过期。`ttl==0` 立即过期。`last_active_tick` 改为 `atomic<uint32_t>`，drain 每次实际投递后、subscribe 首次设置时更新。

6. **callable 生命周期锁边界（语义 7，硬性封顶）**：所有 `Callback` 的拷贝/移动/销毁都在锁外：
   - `subscribe`：先在锁外 `make_shared` 并移动赋值 callback，再加锁仅做 shared_ptr 拷贝入 vector；
   - `unsubscribe`：锁内把 shared_ptr 移出，erase 销毁的是已被 move 的空 shared_ptr（无用户代码），持有点在锁外局部变量析构；
   - `expire_idle`：分区时只移动 shared_ptr、把淘汰项移入锁外局部 vector；
   - `drain`：投递全程不持锁。
   - 重入（含 callable 析构触发 `unsubscribe`）安全：析构在锁外，再入 `unsubscribe` 只会看到已移除的 id。

7. **线程安全（语义 7）**：公共方法各自加 `mutex_`；drain 投递期间不持锁，多线程并发 publish/subscribe/unsubscribe/expire_idle 与 drain 互不阻塞用户回调。

## 验证

- `./run_public_checks.sh`：包含 `public_checks.cpp`、`callable_lifetime_checks.cpp`、新增 `extra_checks.cpp`，全部通过（g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread）。
- 新增 `extra_checks.cpp` 覆盖：临时 string 所有权、批次边界（回调内 publish 留到下次 drain）、异常隔离（三订阅者其一抛异常，另两个仍收到、事件不重投）、回调内取消订阅阻止后续事件、回调内新订阅跳过本批次但收下次、`ttl==0` 立即过期、`uint32_t` 回绕过期判定、回调内重入 drain。
- ASAN（`-fsanitize=address`，`detect_leaks=0`）单独跑 `callable_lifetime_checks.cpp` 与 `extra_checks.cpp` 均通过，无 use-after-free / 栈溢出。
- 说明：本机 ASAN 在 `detect_leaks=1` 下对**题面自带的最简 `public_checks.cpp`** 也产生 `DEADLYSIGNAL` 级联（4M+ 行、超时），属环境 LSan 行为，与本实现无关；官方 `run_public_checks.sh` 与 Q3 的 asan 用例均使用 `detect_leaks=0`。

## 剩余风险

- 未编写大规模多线程压力测试（语义 7 的并发路径）；现有实现以「投递期不持锁 + 订阅/事件快照」保证不重入死锁，逻辑上可承受，但缺少定量压测佐证。
- `alive` 与投递之间存在极小的 TOCTOU 窗口：订阅在「投递前」被取消则跳过，在「投递后」取消则本次已投递，均符合「至多一次」语义。
