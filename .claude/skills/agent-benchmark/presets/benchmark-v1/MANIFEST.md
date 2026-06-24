# benchmark-v1

默认 C++17 工程能力基准题库，版本固定后不得在同一轮 benchmark 内修改。该公开题库不包含参考答案、隐藏测试或评分结论；仅供 scorer 的私有参考解答位于题库外的 `../../scoring-reference/benchmark-v1.md`。

> 2026-06-20 的题面澄清将 callable 生命周期、Clock 重入、flight 完成顺序和跨实例 `current()` 生命周期列为公开契约。它只适用于之后从本预设生成的 benchmark，不追溯改变已归档轮次。

Q1 的 callable 生命周期锁边界是唯一标记为“硬性封顶条件”的预设语义；其余公开语义按正确性、C++ 质量、约束遵循和验证证据评分，不因单项失败自动触发分数上限。

| 题目 | 类型 | 目标时间盒 | 核心能力 |
|------|------|------------|----------|
| `q01-subscription-hub` | 组合 Bugfix | 50 分钟 | 所有权、重入、并发边界、异常安全、时钟回绕 |
| `q02-coalescing-cache` | Implementation | 70 分钟 | 并发状态机、缓存一致性、LRU、失效语义、测试设计 |
| `q03-routing-config` | Refactor/Design | 60 分钟 | API 兼容、快照发布、生命周期、回调隔离、可测试性 |

## 使用规则

- 生成 benchmark 时完整复制三个题目目录到公共 `questions/`，再分别复制到每个 `agents/<slug>/` 作答目录。
- `QUESTION.md`、starter code、`ANSWER.md` 和 `tests/` 都是公开材料；不得额外补充私有题面或隐藏答案。
- `tests/` 同时包含基础路径和公开的关键边界检查；评分以 `QUESTION.md` 的完整验收标准、参评代码与验证记录为准。
- 三道题的 starter 都存在公开可复现的缺陷：Q1 的 callable 析构重入、Q2 的未实现骨架、Q3 的跨实例生命周期/observer 复制重入。因此完整 `run_public_checks.sh` 在 starter 状态均预期失败；这不是对任何参评对象的预评分。
- 私有评分参考解答只在全部参评对象停止作答后复制到本轮 `scoring/scorer-reference/`；绝不能复制到 `questions/`、`agents/` 或 `submissions/`。
