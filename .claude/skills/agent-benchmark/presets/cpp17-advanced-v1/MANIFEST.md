# cpp17-advanced-v1

默认 C++17 工程能力基准题库，版本固定后不得在同一轮 benchmark 内修改。该公开题库不包含参考答案、隐藏测试或评分结论；仅供 scorer 的私有参考解答位于题库外的 `../../scoring-reference/cpp17-advanced-v1.md`。

| 题目 | 类型 | 目标时间盒 | 核心能力 |
|------|------|------------|----------|
| `q01-subscription-hub` | 组合 Bugfix | 50 分钟 | 所有权、重入、并发边界、异常安全、时钟回绕 |
| `q02-coalescing-cache` | Implementation | 70 分钟 | 并发状态机、缓存一致性、LRU、失效语义、测试设计 |
| `q03-routing-config` | Refactor/Design | 60 分钟 | API 兼容、快照发布、生命周期、回调隔离、可测试性 |

## 使用规则

- 生成 benchmark 时完整复制三个题目目录到公共 `questions/`，再分别复制到每个 `agents/<slug>/` 作答目录。
- `QUESTION.md`、starter code、`ANSWER.md` 和 `tests/` 都是公开材料；不得额外补充私有题面或隐藏答案。
- `tests/public_checks.cpp` 只覆盖基础行为，不能作为完整验收。评分以 `QUESTION.md` 的完整验收标准、参评代码与验证记录为准。
- `q02` 的 starter 特意只提供可编译骨架，公开检查在未实现前会失败；其余 starter 可能通过基础公开检查但仍包含题面描述的生产缺陷。
- 私有评分参考解答只在全部参评对象停止作答后复制到本轮 `scoring/scorer-reference/`；绝不能复制到 `questions/`、`agents/` 或 `submissions/`。
