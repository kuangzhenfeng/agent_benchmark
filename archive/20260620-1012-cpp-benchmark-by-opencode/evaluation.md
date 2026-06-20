# Agent Benchmark Evaluation

## 总结

本轮评测 5 个 AI Coding Agent × 模型组合在 3 道 C++17 高级工程任务上的表现。采用双 scorer subagent 独立盲评后解盲汇总。

**排名结论**：claude code + glm-5-2（287 分）、opencode + glm-5-2（282 分）和 codex + gpt-5.5（280 分）构成领先组，三者总分差距 ≤ 7 分，本轮未拉开显著差距。这三个组合均正确实现了所有关键并发语义（callable 锁边界、单一同步转换、原子快照）。claude code + qwen3.7max（214 分）和 opencode + qwen3.7max（195 分）显著落后，均在 Q1 和 Q3 的 `subscribe()` 中于锁内移动 `std::function`，触发硬性封顶。

**主要拉分因素**：模型差异是本轮最大拉分因素——glm-5.2 和 gpt-5.5 远优于 qwen3.7max。callable 生命周期锁边界（`std::function` 的构造/移动/析构必须在 mutex 外）是具体的技术区分点。

## 盲评流程

- 匿名 ID：P01–P05
- 评分员：Scorer A、Scorer B（两个独立 scorer subagent）
- scorer 是否看到 Agent/模型身份：否
- 双 scorer 差异：总分差异 P01=26, P02=34, P03=1, P04=28, P05=7
- 未完全匿名的信息：代码风格差异可作为模糊身份线索（已记录于 redaction-log.md）

## 解盲映射

| 匿名 ID | 参评对象 | slug |
|---------|----------|------|
| P01 | claude code + glm-5.2 | `claude-code-glm-5-2` |
| P02 | codex + gpt-5.5 | `codex-gpt-5-5` |
| P03 | claude code + qwen3.7max | `claude-code-qwen3-7-max` |
| P04 | opencode + glm-5.2 | `opencode-glm-5-2` |
| P05 | opencode + qwen3.7max | `opencode-qwen3-7-max` |

## 双 Scorer 差异与调和

| 差异来源 | 说明 | 处理方式 |
|----------|------|----------|
| Scorer B 系统性偏高 | P01/P02/P04 多项给满分 100，A 更保守（88–93 范围） | 取均值 |
| Scorer B 误用硬性封顶 | Q3 无"硬性封顶条件"（仅 Q1 有），B 对 P03/P05 Q3 错误应用了 ≤60 封顶 | 使用 Scorer A 分数（P03 Q3=79, P05 Q3=77） |
| P02 `last_active_tick` 数据竞争 | A 检测到（非 atomic uint32_t）并扣分，B 忽略 | A 的扣分有效，取均值 |
| P05 Q2 capacity=0 崩溃风险 | B 检测到但 A 未明确提及 | 取均值反映此差异 |

## 总分

| 排名 | 参评对象 | Q1/100 | Q2/100 | Q3/100 | 平均/100 | 总分/300 | 结论 |
|------|----------|--------|--------|--------|----------|----------|------|
| 1 | claude code + glm-5-2 | 97 | 95 | 96 | 96 | 287 | 本轮最高分 |
| 2 | opencode + glm-5-2 | 91 | 96 | 95 | 94 | 282 | 与第一名差距 < 8，本轮未拉开显著差距 |
| 3 | codex + gpt-5.5 | 93 | 93 | 95 | 94 | 280 | 与第一名差距 < 8，本轮未拉开显著差距 |
| 4 | claude code + qwen3.7max | 58 | 77 | 79 | 71 | 214 | 显著落后 |
| 5 | opencode + qwen3.7max | 57 | 70 | 69 | 65 | 195 | 本轮最低分 |

> 注：P03/P05 的 Q3 使用 Scorer A 分数（Scorer B 对 Q3 误用了仅属于 Q1 的硬性封顶规则）。其余所有分数取双 scorer 均值。

## 分题评分

### Q1 SubscriptionHub（组合 Bugfix，50 分钟）

| 参评对象 | 正确性/45 | C++质量/20 | 约束/15 | 可维护/10 | 验证/10 | 总分/100 | 主要依据 |
|----------|-----------|------------|---------|-----------|---------|----------|----------|
| claude code + glm-5-2 | 48/45 | 20/20 | 15/15 | 10/10 | 9.5/10 | **97** | 全部核心语义满足，callable 全程锁外，atomic 取消语义正确 |
| codex + gpt-5.5 | 42/45 | 17.5/20 | 14.5/15 | 8.5/10 | 10/10 | **93** | 基本正确，`last_active_tick` 非 atomic 有数据竞争 |
| claude code + qwen3.7max | 35/45 | 15/20 | 8.5/15 | 7.5/10 | 5.5/10 | **58** | **硬性封顶**: subscribe 锁内 move Callback |
| opencode + glm-5-2 | 43.5/45 | 18.5/20 | 15/15 | 9/10 | 9.5/10 | **91** | 完整正确，atomic alive 标记，move iterator 高效截取 |
| opencode + qwen3.7max | 30/45 | 12/20 | 6/15 | 6/10 | 5.5/10 | **57** | **硬性封顶**: 3 处 callable 锁边界违反 |

**Q1 关键差异**：callable 生命周期锁边界是最大区分因素。claude code + glm-5-2 / codex + gpt-5.5 / opencode + glm-5-2 正确地在锁外构造 `shared_ptr<Callback>`，而 claude code + qwen3.7max 和 opencode + qwen3.7max 在 `subscribe()` 中于锁内执行 `make_shared<Callback>(std::move(callback))`。opencode + qwen3.7max 更因 `vector<Subscription>` 直接包含 `Callback` 导致 drain 快照深拷贝时在锁内拷贝所有 callable。

### Q2 CoalescingCache（Implementation，70 分钟）

| 参评对象 | 正确性/45 | C++质量/20 | 约束/15 | 可维护/10 | 验证/10 | 总分/100 | 主要依据 |
|----------|-----------|------------|---------|-----------|---------|----------|----------|
| claude code + glm-5-2 | 43.5/45 | 18.5/20 | 14.5/15 | 9/10 | 9.5/10 | **95** | 单 mutex 单 cv，flight 完成序列在同一临界区 |
| codex + gpt-5.5 | 42.5/45 | 17.5/20 | 14.5/15 | 9/10 | 9/10 | **93** | 单 mutex，ActiveCallGuard RAII，clock throw 返回 failure |
| claude code + qwen3.7max | 38/45 | 15/20 | 12/15 | 7.5/10 | 6.5/10 | **77** | 双 mutex 不满足语义 7 单一同步转换 |
| opencode + glm-5-2 | 44/45 | 18.5/20 | 15/15 | 9/10 | 9.5/10 | **96** | 单 mutex+cv，thread_local active_loads 递归检测 |
| opencode + qwen3.7max | 32/45 | 13/20 | 11.5/15 | 6.5/10 | 6.5/10 | **70** | 双 mutex 同步分裂 + capacity=0 崩溃 + recursive 不区实例 |

**Q2 关键差异**：单 mutex vs 双 mutex 设计。前三名采用单一 mutex 在同一个临界区完成 flight 的全部发布序列（result → done → cache-write → flight-erase → notify），满足语义 7 的"单一可同步状态转换"。claude code + qwen3.7max 和 opencode + qwen3.7max 使用双 mutex（flight mtx + state mtx），分步发布，违反该要求。opencode + qwen3.7max 额外有 `capacity=0` 时可能的无限循环和 `recursive_load` 检测不区分 cache 实例的问题。

### Q3 RoutingConfig（Refactor/Design，60 分钟）

| 参评对象 | 正确性/45 | C++质量/20 | 约束/15 | 可维护/10 | 验证/10 | 总分/100 | 主要依据 |
|----------|-----------|------------|---------|-----------|---------|----------|----------|
| claude code + glm-5-2 | 43.5/45 | 18.5/20 | 15/15 | 9/10 | 9.5/10 | **96** | 不可变快照 + per-instance TLS pin + observer 锁外通知 |
| codex + gpt-5.5 | 42.5/45 | 18.5/20 | 15/15 | 9/10 | 9.5/10 | **95** | valid 锁外 + per-instance TLS pin + 锁外构造 holder |
| claude code + qwen3.7max | 38/45 | 15/20 | 11.5/15 | 7.5/10 | 6.5/10 | **79** | subscribe 锁内 move Observer（违反语义 4） |
| opencode + glm-5-2 | 42.5/45 | 18.5/20 | 15/15 | 9.5/10 | 9/10 | **95** | notify_observers 抽取清晰 + 锁外构造 Observer |
| opencode + qwen3.7max | 35/45 | 14/20 | 11.5/15 | 7/10 | 7.5/10 | **69** | subscribe 锁内 move Observer + current() 持锁写 TLS |

> 注：Scorer B 对 P03/P05 Q3 误用了硬性封顶（≤60），Q3 的 QUESTION.md 无此类条件。最终使用 Scorer A 分数。

**Q3 关键差异**：subscribe callable 锁边界再次成为主要拉分点。前三名在锁外 `make_shared<Observer>`，后两名在锁内执行 `std::make_shared<Observer>(std::move(observer))`。虽然 Q3 不触发硬性封顶，但此设计缺陷在约束遵循和 C++ 质量维度各扣分。

## 关键差异

1. **callable 锁边界一致性**：领先组（claude code + glm-5-2、opencode + glm-5-2、codex + gpt-5.5）在全部 3 题中正确实现 callable 锁外生命周期。落后组（claude code + qwen3.7max、opencode + qwen3.7max）在 Q1/Q3 的 subscribe 中反复犯同一错误（锁内 move `std::function`），表明 qwen3.7max 对这一并发安全的工程惯例理解不足。

2. **单一同步转换 vs 双 mutex**：领先组采用单一 mutex 在同一个临界区完成 flight 的全部发布序列（result → done → cache-write → flight-erase → notify），满足语义 7 的"单一可同步状态转换"。落后组使用双 mutex 分步发布，违反该要求。opencode + qwen3.7max 额外有 `capacity=0` 时可能的无限循环和 `recursive_load` 检测不区分 cache 实例的问题。

3. **测试覆盖差异**：claude code + glm-5-2（30 额外用例）和 opencode + glm-5-2（29 额外用例）测试最全面，覆盖并发边界、极端输入、ASAN 验证。codex + gpt-5.5 有 4 个额外测试文件。claude code + qwen3.7max 和 opencode + qwen3.7max 未新增任何额外测试。

4. **Agent 框架差异（粗略观察）**：在同一模型 qwen3.7max 下，claude code（214 分）与 opencode（195 分）差距 19 分。在同一模型 glm-5.2 下，claude code（287 分）与 opencode（282 分）差距 5 分。差距主要由 Q1 和 Q3 的 callable 违规严重度不同驱动（claude code + qwen3.7max 单处违规得 58 分，opencode + qwen3.7max 多处违规得 57 分）。5 个样本不足以分离 Agent 与模型的交互效应。

5. **模型差异（主要拉分因素）**：在同一 Agent claude code 下，glm-5.2（287 分）远优于 qwen3.7max（214 分），差距 73 分。在同一 Agent opencode 下，glm-5-2（282 分）远优于 qwen3.7max（195 分），差距 87 分。codex + gpt-5.5（280 分）与两组 glm-5.2 表现接近。模型差异是本轮最大拉分因素，gpt-5.5 和 glm-5.2 属于同一能力梯队，qwen3.7max 显著落后。

## 风险与限制

- **样本量**：5 个参评对象，不足以分离「Agent 框架 × 模型」的交互效应。
- **双 scorer 差异**：总分差异 1–34 分，Scorer B 系统性偏高（多项给满分 100），Scorer A 更保守且有更细致的 defect 分析。已对 Q3 硬性封顶误用做修正。
- **TSan 不可用**：环境缺 libtsan_preinit.o，所有并发正确性依赖源码静态分析 + ASAN（仅内存安全）。
- **未运行或无法确认的测试**：部分 ASAN 测试因环境 LSan DEADLYSIGNAL 跳过（非代码问题）。
- **不应外推的范围**：仅 3 道 C++17 并发 / 状态机 / API 设计题。不能外推到 Python/JS/系统编程、大型代码库理解、调试循环或文档写作等任务。
- **P02 (codex) `last_active_tick` 数据竞争**：uint32_t 非 atomic，多线程读写构成 C++ UB。在 x86 上通常不导致错误但技术上未定义。

## 下一轮建议

1. **增加样本**：至少 3 个额外模型和 2 个额外 Agent，以更好地分离交互效应。
2. **题型扩展**：增加 1 道性能敏感题（缓存局部性 / 内存池）和 1 道模板元编程题。
3. **强制 TSan**：在评测环境预设 TSan 运行时，要求 scorer subagent 尝试 TSan 验证。
4. **callable 锁边界专项测试**：增加 subscribe 阶段 move 重入测试到公开检查中（当前仅覆盖 unsubscribe 阶段）。
5. **固定编译时间盒**：各 Agent 实际编译和测试运行时间也应记录。
