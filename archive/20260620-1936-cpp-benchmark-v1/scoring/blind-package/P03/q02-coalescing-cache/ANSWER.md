# Q2 答案说明

## 实现要点

1. **并发请求合并**：对同一 key 的并发 `get_or_load`，第一个调用者成为 leader 执行 loader，其他调用者等待同一 flight。

2. **在途状态所有者**：flight 对象（`std::shared_ptr<Flight>`）由 leader 创建并持有，所有等待者共享同一 flight。flight 包含 `mutex`、`condition_variable`、`done` 标志和结果。

3. **等待者唤醒顺序**：所有等待者通过 `cv.wait()` 等待 `done` 标志，leader 完成 loader 后设置 `done = true` 并 `notify_all()`，所有等待者同时被唤醒。

4. **invalidate 竞态处理**：
   - `invalidate` 从 ready cache 中移除对应 key
   - 同时标记在途 flight 为 `invalidated`
   - loader 完成后检查 `invalidated`，若已标记则不写入 ready cache
   - 但等待者仍能得到该次结果

5. **LRU 淘汰**：ready cache 使用 `std::list` + `std::unordered_map` 实现 LRU，访问时 `splice` 到队首，淘汰时从队尾移除。

6. **锁边界**：
   - `Clock` 在锁外执行（检查缓存命中/过期时调用）
   - `loader` 在锁外执行（leader 执行 loader 时无锁）
   - `invalidate` 内部持有锁，但 `Clock` 重入调用 `invalidate` 时不会死锁（因为 `Clock` 在锁外调用）

7. **TTL 为 0**：不缓存成功结果，但并发调用仍合并同一次在途加载。

8. **loader 异常**：捕获异常并转为 `LoadResult::failed`，不跨越 `get_or_load` 的 API 边界。
