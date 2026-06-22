# cpp17-advanced-v1 评分私有参考解答

仅供 benchmark 生成完成、所有参评对象停止作答后，由主 agent 复制给匿名 scorer。不得复制到 `questions/`、`agents/`、`submissions/`，不得在评分前向参评 Agent 透露其内容。

本文件是语义参考解答和验收矩阵，不是唯一源码实现。scorer 必须接受任何满足公开题面全部语义、资源安全和并发约束的不同设计；不得以“未采用本文件的数据结构”为扣分理由。

## Q1：SubscriptionHub

### 参考修复设计

- 订阅 topic 与待投递 event 的 topic/payload 都由 hub 自己拥有，例如在私有 `Subscription`、`QueuedEvent` 中保存 `std::string`。对回调临时构造的 `Event` 可暴露指向这些字符串的 `string_view`，但仅在回调期间有效。
- `drain` 在锁内一次性截取调用开始时的待处理队列，并为本次 drain 冻结初始订阅 ID 顺序；随后释放锁。这样回调 publish 的事件不会进入外层本次批次，新订阅也不进入该批次。
- `subscribe` 在获取 mutex 前把传入 callback 封装为 `shared_ptr<Callback>`，私有订阅记录只保存该指针。每个即将调用的 ID 在短锁内只确认仍存在、topic 仍匹配、复制 shared_ptr，并更新活跃 tick；锁外再解引用并调用 callback。`std::function` 的复制、移动和最后一次销毁都可能执行用户 callable 的特殊成员函数，绝不能发生在 mutex 内。
- `unsubscribe`、过期清理和覆盖订阅记录时，如可能释放最后一个 callback 指针，必须在锁内将其移到局部延迟释放容器，并在解锁后销毁。取消订阅会影响该 ID 尚未开始的后续 event，新订阅不会补入本批次。
- 每次实际 callback 尝试（即使抛异常）都增加 `delivered`；捕获异常并增加 `callback_errors`，继续其他回调。事件在截取时已从待处理队列移走，因此异常不能导致下次重复投递。
- 过期条件使用无符号模减法：`static_cast<uint32_t>(now - last_active) >= ttl`。题面限定 `ttl < 2^31`；TTL 为 0 时无需特殊排除，应立即满足条件。
- 公共状态仅在短锁内访问；题面已公开要求任何 `Callback`、用户 callable 的复制/移动/销毁，以及分配后可能执行用户代码的路径都不能在 mutex 下运行。

### 验收矩阵

| 场景 | 正确结果 | 常见错误 |
|------|----------|----------|
| 临时 `std::string` 后再 drain | topic 与 payload 完整 | 私有字段保存 `string_view` |
| callback 取消自己或其他订阅 | 不死锁；取消项不再收到后续 event | 在锁内 callback；遍历失效 vector 引用 |
| callback 的复制或析构重入 hub | subscribe、drain、unsubscribe、expire 均不死锁 | 在 mutex 内复制或释放 `std::function` |
| callback 发布新 event | 外层 drain 不处理新 event | 直接遍历/清空共享队列 |
| 一个 callback 抛异常 | 其余订阅仍执行；该 event 不重放 | 异常逃逸；事件残留队列 |
| `last=UINT32_MAX-2, now=3` | 依据模差正确判定 TTL | 使用 `now > last` |

## Q2：CoalescingCache

### 参考实现状态机

每个 key 只能处于以下组合状态：无记录、ready cache entry、in-flight flight，或在 invalidate 后的 in-flight-but-not-cacheable。典型实现使用一个 mutex 保护 `unordered_map`、LRU 链表和 `shared_ptr<Flight>`；`Flight` 至少包含 `done`、`LoadResult`、`condition_variable` 与 `cacheable` 标志。`Clock` 与 loader 同属用户 callable，必须在内部 mutex 外调用。

1. 在锁外取得 Clock 值，再在锁内读取 ready entry；未过期则刷新 LRU 并返回。过期 entry 删除后继续查找/创建 flight。
2. 若存在 flight，调用者成为 waiter，释放锁并在 flight 条件变量上等待完成；不得执行 loader。
3. 若不存在 flight，创建并登记 flight 后成为 owner。owner 在锁外调用 loader，捕获所有异常并转换为 `loader_failed`。
4. owner 在锁外取得**完成时** Clock 值，然后在同一受同步保护的完成转换中写入最终结果、完成标志及 ready cache 决定。仅在成功、TTL 非零且 flight 未被 invalidate 标记时写入 ready entry。执行容量淘汰时仅淘汰 ready entry；全部在途时允许暂不缓存，不得破坏 flight。
5. owner 只在最终结果和 ready cache 决定均已发布后唤醒所有 waiter；在 flight 从 in-flight map 移除前，所有找到它的调用者必须能经同一同步关系读到完整结果。所有 waiter 与 owner 获得同一 `LoadResult` 内容；失败不缓存，且 flight 最终从 in-flight map 移除。
6. `invalidate` 删除 ready entry；若 flight 已存在，保留它供当前和后续调用者合并，但设置不缓存标志。它不取消或第二次启动 loader。
7. 同线程、同 cache 实例、同 key 的 owner 重入调用必须在尝试 wait 前被识别，并立即返回 `recursive_load`。可使用 thread-local `(cache instance, key)` 加载集合并采用作用域清理。

### 验收矩阵

| 场景 | 正确结果 | 常见错误 |
|------|----------|----------|
| N 个并发同 key miss | loader 恰好 1 次，N 个等价结果 | 每线程自行加载；锁包住 loader |
| loader 请求不同 key | 可完成，无全局死锁 | 全局 mutex 在 loader 期间持有 |
| Clock 调用 `invalidate` | 可完成，无全局死锁 | 在 cache mutex 内调用 Clock |
| loader 请求同 key | 立即 `recursive_load` | 等待自己造成死锁 |
| loader 抛异常 | 全部 waiter 得失败；后续可重试 | 异常逃逸或 flight 永不通知 |
| TTL 0 | 该次合并可用，之后不命中 ready | 把结果写入 cache |
| in-flight 中 invalidate | 当前 flight 返回但不写 ready | 结果完成后重新污染 cache |
| 完成中的晚到调用 | 安全获得已发布结果，或在 flight 已不可发现且无 ready 时新建 flight | 读取未同步的 `done/result` |
| capacity 小于 ready 数 | 命中刷新 LRU；只淘汰最久未用 ready | 淘汰 flight 或 LRU 顺序错误 |

## Q3：RoutingConfig

### 参考重构设计

- 将已发布配置表示为 `shared_ptr<const Config>`。成功 reload 先在局部副本完整校验，再构造新 immutable snapshot，将版本设为旧版本加一，并通过短 mutex 临界区或 C++17 的 `atomic_store`/`atomic_load` shared_ptr 自由函数一次发布。
- `snapshot()` 获取一个 shared_ptr 并直接返回；`find_target` 只对该单一 snapshot 执行最长 prefix 匹配。
- 题面已要求：为保留 `current()` 签名，调用线程使用 `thread_local` 的 per-instance `shared_ptr<const Config>` pin 保存刚取得 snapshot，再返回 `*pin`。因此 reload 不会销毁该线程在同一实例下次 `current()` 前持有引用所指对象；查询另一实例不能替换此 pin。仅返回 mutex 内部 `Config&` 或局部 shared_ptr 解引用均不合格。
- `subscribe` 在获取 mutex 前把传入 observer 封装为 `shared_ptr<Observer>`，内部 map 保存该指针。reload 在短锁内只取得观察者 ID 与 shared_ptr 的固定副本及已发布 snapshot，随后完全释放锁再逐个解引用通知；题面已明确不得在锁内复制、移动或销毁 `std::function`。unsubscribe 及替换/清理观察者时必须把可能成为最后一个引用的 shared_ptr 延迟到解锁后销毁。捕获每个异常；异常计数在短锁内增加。回调内 unsubscribe 仅影响未来 reload，subscribe 不进入已固定的通知快照。
- 失败校验绝不发布、绝不递增版本、绝不通知。重复 prefix、空 prefix、空 target 均失败。

### 验收矩阵

| 场景 | 正确结果 | 常见错误 |
|------|----------|----------|
| 无效 candidate | `false`，版本/快照/通知均不变 | 先修改再校验，出现半更新 |
| reload 后旧 snapshot/current 引用 | 仍可安全读取旧完整配置 | 返回内部可变对象引用 |
| 同线程跨两个实例调用 `current()` | 第一个实例的旧引用仍可安全读取 | 单个 TLS pin 被第二个实例覆盖 |
| observer 调用 unsubscribe/reload | 不死锁；当前轮通知集合不变 | 持锁回调或迭代可变 map |
| observer 的复制或析构重入配置 API | subscribe、reload、unsubscribe 均不死锁 | 在 mutex 内复制或释放 `std::function` |
| observer 抛异常 | 其他 observer 仍通知，错误计数增加 | 异常打断发布或通知 |
| `/v1/users` 与 `/`、`/v1` | 返回 `/v1` 的 target | 首个匹配而非最长 prefix |
| 并发 reload/read | reader 只见完整单一版本 | 就地 mutate vector/routes |

## 评分使用规则

- 先以公开 `QUESTION.md` 为准；本文件只消除歧义、提示组合场景和帮助识别伪修复。
- 参评者采用等价实现时，按行为和证据评分，不按本文件的类名、锁类型或容器选择评分。
- 遇到不能从源码或验证记录确定的并发行为，明确标注为未验证风险并相应扣减“验证证据”；不得臆测通过。
- 本文件不得出现在最终面向参评者的报告、任何 `agents/` 目录或公共题源中。
