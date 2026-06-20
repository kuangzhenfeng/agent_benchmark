# Q3 答案说明

## 重构要点

1. **原子发布**：`reload()` 在校验通过后，在锁内原子替换 `current_` 为新的 `shared_ptr<const Config>`，然后收集观察者列表并在锁外通知。

2. **current() 引用保活**：
   - 使用 `thread_local std::map<const RoutingConfig*, std::shared_ptr<const Config>>` 为每个实例维护独立的 TLS pin
   - `current()` 在锁内将当前配置 pin 到 TLS map 中对应实例的槽位
   - 返回 `*current_` 的引用，该引用由 TLS 中的 shared_ptr 保活
   - 析构时清理该实例在 TLS 中的 pin，避免内存泄漏
   - 跨实例调用 `current()` 互不干扰，每个实例有独立的 pin 槽

3. **观察者快照与异常隔离**：
   - `reload()` 在锁内收集观察者快照（复制 `Observer` 到 vector）
   - 在锁外遍历通知观察者，每个 observer 抛异常不影响其他 observer
   - 异常次数通过 `observer_errors_` 计数

4. **observer callable 重入安全**：
   - 锁内只复制 observer 到 vector（只复制 `std::function` 对象，不调用）
   - 锁外才调用 observer，避免 callable 的复制/调用在锁内执行
   - `ReenterOnCopy` 测试通过在锁外复制 observer 来避免死锁

5. **find_target**：使用 `snapshot()` 获取一致快照后查询，避免读到部分更新的配置。

6. **版本管理**：每次成功 reload 后 `version` 严格加一。
