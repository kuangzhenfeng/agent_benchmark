# Agent Benchmark Evaluation

## 总结

按总平均分，`codex + gpt-5.5` 位列第一（95.67），但与 `opencode +
glm-5.2`（94.67）和 `claude code + glm-5.2`（93.00）的分差均低于 8 分
阈值。因此，本轮的合理结论是：前三者在这组三道 C++17 任务上**未拉开
显著差距**，而不是第一名已被充分证明存在总体优势。两个 Qwen3.7 Max
组合在 Q1 均触发题面硬性封顶条件，且在 Q2/Q3 保留了更多并发或 callable
生命周期缺口；这是本轮的主要分层来源。

## 盲评流程

- 匿名 ID：P01–P05。
- 评分员：独立 scorer subagent。
- scorer 是否看到 Agent/模型身份：否；`mapping.private.md` 未提供给 scorer。
- 评分材料：完整匿名源码、题面、`ANSWER.md`、提交附带测试和私有预设参考。
- 验证：scorer 已编译并运行常规 C++17 公开检查和提交附带测试。
- 未完全匿名的信息：实现风格、测试选择或自述可能造成身份猜测；评分员被要求不作此类推断。
- 完整匿名评分依据见 [scorer-report.md](scoring/scorer-report.md)。

## 总分

| 排名 | 参评对象 | Q1 | Q2 | Q3 | 总分 / 300 | 平均分 | 结论 |
|---:|---|---:|---:|---:|---:|---:|---|
| 1 | codex + gpt-5.5 | 95 | 96 | 96 | 287 | 95.67 | 三题均没有 scorer 识别出的核心语义缺口；与第 2、3 名差距不显著。 |
| 2 | opencode + glm-5.2 | 95 | 93 | 96 | 284 | 94.67 | 整体完整；Q2 对后续 `ttl=0` 调用的 ready 命中语义偏弱。 |
| 3 | claude code + glm-5.2 | 96 | 87 | 96 | 279 | 93.00 | Q1/Q3 完整；Q2 过期并发路径可能出现双 owner。 |
| 4 | claude code + qwen3.7max | 60 | 88 | 82 | 230 | 76.67 | Q1 触发 callable 锁边界硬性封顶；Q2、Q3 仍有可定位的并发/生命周期问题。 |
| 5 | opencode + qwen3.7max | 60 | 69 | 81 | 210 | 70.00 | Q1 同样触发硬性封顶；Q2 的 TTL 归属与跨实例递归语义不足。 |

## 分题评分

### Q1 — subscription-hub

| 参评对象 | 正确性/45 | C++质量/20 | 约束/15 | 可维护/10 | 验证/10 | 总分 | 主要依据 |
|---|---:|---:|---:|---:|---:|---:|---|
| claude code + qwen3.7max | 43 | 12 | 0 | 7 | 5 | 60* | `subscribe` 在锁内移动 `Callback`，违反硬性 callable 生命周期条件。 |
| opencode + qwen3.7max | 42 | 8 | 0 | 5 | 7 | 60* | `subscribe`、`push_back`、订阅快照均在锁内复制或移动 callable。 |
| claude code + glm-5.2 | 44 | 19 | 15 | 9 | 9 | 96 | shared ownership、批次隔离与锁外 callable 生命周期完整。 |
| opencode + glm-5.2 | 44 | 19 | 15 | 9 | 8 | 95 | 语义完整；新增测试实际未被脚本执行，验证证据小扣分。 |
| codex + gpt-5.5 | 44 | 18 | 15 | 9 | 9 | 95 | 锁外 owner、ID cutoff 和按事件投递列表满足组合语义。 |

`*`：题面硬性封顶条件触发，该题总分上限为 60。

### Q2 — coalescing-cache

| 参评对象 | 正确性/45 | C++质量/20 | 约束/15 | 可维护/10 | 验证/10 | 总分 | 主要依据 |
|---|---:|---:|---:|---:|---:|---:|---|
| claude code + qwen3.7max | 40 | 18 | 15 | 9 | 6 | 88 | 在 cache/in-flight 删除之前先发布 `done`，破坏单一完成转换。 |
| opencode + qwen3.7max | 28 | 13 | 15 | 6 | 7 | 69 | 命中时采用当前调用 TTL；递归检测没有区分 cache 实例。 |
| claude code + glm-5.2 | 36 | 18 | 15 | 9 | 9 | 87 | 过期路径在 Clock 回来后未复查 in-flight，可能并发双 owner。 |
| opencode + glm-5.2 | 43 | 19 | 15 | 9 | 7 | 93 | 单 mutex 状态转换完整；后续 `ttl=0` 可能跳过已有 ready entry。 |
| codex + gpt-5.5 | 44 | 19 | 15 | 9 | 9 | 96 | active-call guard、完成转换、TTL/LRU/失效和异常语义一致。 |

### Q3 — routing-config

| 参评对象 | 正确性/45 | C++质量/20 | 约束/15 | 可维护/10 | 验证/10 | 总分 | 主要依据 |
|---|---:|---:|---:|---:|---:|---:|---|
| claude code + qwen3.7max | 41 | 16 | 8 | 9 | 8 | 82 | `subscribe` 在 mutex 内构造/移动 `Observer`。 |
| opencode + qwen3.7max | 41 | 16 | 8 | 9 | 7 | 81 | `subscribe` 在 mutex 内 `make_shared` 并移动 `Observer`。 |
| claude code + glm-5.2 | 44 | 19 | 15 | 9 | 9 | 96 | immutable snapshot、per-instance TLS pin、observer 生命周期均完整。 |
| opencode + glm-5.2 | 44 | 20 | 15 | 9 | 8 | 96 | 发布、`current()` 保活、observer 隔离和最长前缀匹配完整。 |
| codex + gpt-5.5 | 44 | 19 | 15 | 9 | 9 | 96 | per-instance TLS pin、锁外 observer 生命周期与匹配语义完整。 |

## 关键差异

- Q1 的分层不是基础功能，而是 callable 特殊成员函数的锁边界。两个 Qwen3.7 Max 提交均通过常规路径，却在锁内 copy/move `Callback`，因此触发明确的 60 分封顶；其余三份提交将 callable owner 的构造、移动或析构移出锁区。
- Q2 是最能区分并发状态机深度的一题。前三名候选分别正确处理了 flight 单一完成转换、递归实例边界和 ready-entry 的 TTL；P03 仍有 Clock 后不复查 in-flight 的竞态，P04 有 `ttl=0` ready 命中边界，P05 未发现相应核心缺口。
- Q3 的快照发布和最长前缀匹配在所有提交中基本到位；主要差异仍是 `Observer` copy/move 必须在锁外。两个 Qwen3.7 Max 提交未满足这一约束。
- 在同一模型的 Agent 框架对比中，Qwen3.7 Max 两组差 6.67 分、GLM-5.2 两组差 1.67 分，均低于 8 分阈值；本轮不能据此宣称某个 Agent 框架有稳定优势。

## 风险与限制

- 样本量为 5，且只有三种固定题型；不应外推到其他语言、产品开发、仓库导航或工具调用场景。
- 时间盒和作答阶段的工具权限无法由当前归档完整审计；结论只评价提交结果，不把耗时或工具能力归因为模型能力。
- scorer 执行了常规检查与提交附带测试，但未执行 TSan；Q3 的 ASan 有非确定性超时/`DEADLYSIGNAL`，未把该现象用于候选人之间扣分。
- 私有参考解答仅用于校准公开语义；允许行为等价实现。匿名并非绝对，代码风格可能构成残余识别风险。

## 下一轮建议

- 增加独立样本，并对 Q2 类并发状态机引入可重复的压力/调度测试，以确认 P03、P04 的边界结论及前三名的稳定性。
- 若目标是比较 Agent 框架而非模型，应做同模型、同权限、同时间盒的配对重复测评，并随机化作答顺序。
- 对 `ASan`/`TSan` 验证固定可复现命令和超时策略，避免环境不稳定被误计入验证能力差异。
