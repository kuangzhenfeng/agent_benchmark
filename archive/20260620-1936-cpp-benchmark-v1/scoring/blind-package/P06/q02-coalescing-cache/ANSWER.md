# Q2 ANSWER

## 修复概要

从零实现了 `CoalescingCache`，满足并发请求合并、LRU 淘汰、TTL 过期、loader 异常隔离、invalidate 竞态、Clock/Loader 锁外执行和递归加载检测等语义。

## 关键设计

### 在途状态（Flight）的所有者
- 第一个发现 key 在 ready cache 和 in-flight 表中都不存在的调用者成为 owner
- owner 创建 `Flight` 对象并通过 `shared_ptr` 放入 `inflight` 表
- owner 在锁外调用 loader，在锁外调用 Clock
- owner 完成后在 state mutex 下原子地：设置结果、决定是否写入 ready cache、从 inflight 中移除、标记 done

### 等待者唤醒顺序
- 非 owner 调用者在 state mutex 下获取 Flight 的 shared_ptr，释放 state mutex 后在 Flight 自己的 mutex 上 `cv.wait` 直到 `done`
- owner 在设置 `done = true` 后释放所有锁，然后 `cv.notify_all()` 唤醒所有等待者
- 每个等待者拿到相同的 `LoadResult`

### invalidate 竞态处理
- `invalidate` 在 state mutex 下移除 ready cache 条目，获取 in-flight 的 Flight shared_ptr
- 释放 state mutex 后锁 Flight mutex 设置 `invalidated = true`
- owner 完成时在 state mutex 和 Flight mutex 下检查 `invalidated` 标志：如果为 true，不写入 ready cache
- 锁序一致（state mutex -> Flight mutex），无死锁
- invalidate 不取消 loader，结果仍返回给等待者

### 递归加载检测
- 使用 `thread_local unordered_set<string>` 跟踪当前线程正在加载的 key
- loader 返回前插入 key，完成后移除
- 内层 `get_or_load` 发现 key 已在 active_loads 中时立即返回 `LoadStatus::recursive_load`

### LRU 淘汰
- `list<string>` + `unordered_map<string, list::iterator>` 实现 O(1) LRU
- 访问 ready 值时移到尾部；插入时容量不足则淘汰头部
- 容量 0 时永不写入 ready cache，但 in-flight 合并正常工作

### 锁边界
- Clock（`state_->now`）在 state mutex 外调用
- Loader 在所有锁外调用
- Flight mutex 只在 state mutex 之后获取（一致的锁序）

## 验证

```
bash run_public_checks.sh  # public_checks（并发合并）+ clock_reentrancy_checks（Clock 重入 invalidate）均通过
```

## 已知风险

- `thread_local` 集合在大量不同 key 的短生命周期加载中可能有轻微开销
- 容量极大时 LRU list 的内存开销线性增长
