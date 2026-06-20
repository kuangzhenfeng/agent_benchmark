# 作答说明

## 修改摘要

1. **状态机设计**：
   - `ready_cache`: 已缓存的有效值，使用 LRU list 管理
   - `in_flight`: 正在加载的 flight，包含结果和 condition_variable

2. **锁边界**：
   - `state_->now()` 在所有锁之外调用，确保 Clock 可重入调用 invalidate
   - loader 在没有任何锁的情况下执行
   - 使用 shared_mutex 保护 ready_cache 和 in_flight

3. **并发请求合并**：
   - 第一个请求创建 flight，其他并发请求等待在 condition_variable 上
   - loader 完成时设置 result 和 done 标志，通知所有等待者

4. **失效与 LRU**：
   - invalidate 只删除 ready_cache，不影响 in_flight
   - LRU 使用 list + unordered_map 实现，容量满时淘汰最旧条目

5. **TTL=0 处理**：
   - 不缓存失败结果，但并发调用仍合并同一次在途加载

## 验证

```bash
cd q02-coalescing-cache && ./run_public_checks.sh
```
- public_checks.cpp: 并发加载合并测试通过
- clock_reentrancy_checks.cpp: Clock 重入调用 invalidate 测试通过

## 剩余风险

1. **loader 重入同一 key**: 当前实现会在内层调用返回 LoadStatus::recursive_load，因为 in_flight 中已存在该 key 的 flight。

2. **flight 内存安全**: flight 使用 shared_ptr 管理，等待者通过 flag 机制避免访问已销毁的 flight。
