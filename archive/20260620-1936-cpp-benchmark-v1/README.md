# Agent Benchmark · 20260620-1936-cpp-benchmark-v1

本轮评测 **opencode + 6 个 coding 模型**在标准 C++17 高级工程任务上的效果。预设：`cpp17-advanced-v1`。

## 总览

| 项 | 内容 |
|----|------|
| 预设 | `cpp17-advanced-v1` |
| 题量 | 3 题（Q1 subscription-hub、Q2 coalescing-cache、Q3 routing-config） |
| 语言标准 | C++17 |
| 编译器 | g++（记录实际版本见 participants.md） |
| 时间盒 | Q1 50 分钟、Q2 70 分钟、Q3 60 分钟（每题独立计时，自动化运行以墙钟上限控制） |
| 参评对象数 | 6 |
| Provider | 全部经 `xag`（https://tokens.xa.com/v1） |
| 公开检查 | 各题目录内 `run_public_checks.sh` |

## 题目

| 题号 | 短名 | 类型 | 推荐耗时 | 评测重点 |
|------|------|------|----------|----------|
| Q1 | subscription-hub | 组合 Bugfix | 50 分钟 | 所有权、回调重入、异常安全、32 位时钟回绕、并发批次语义 |
| Q2 | coalescing-cache | Implementation | 70 分钟 | 并发状态机、请求合并、TTL、失效竞态、LRU、递归加载 |
| Q3 | routing-config | Refactor/Design | 60 分钟 | 快照发布、兼容 API 生命周期、观察者隔离、原子 reload、最长前缀匹配 |

## 公开检查初始状态（预设声明，非预评分）

- **Q1 `subscription-hub`**：基础路径可通过；完整公开套件含 callable 生命周期检查，因 starter 的 callable 生命周期缺陷预期失败。
- **Q2 `coalescing-cache`**：starter 未实现核心状态机，公开检查预期失败。
- **Q3 `routing-config`**：基础路径可通过；完整公开套件因生命周期/重入缺陷预期失败。

> 该声明仅说明 starter 起点，不代表对任何参评对象的预评分。

## 如何分发与作答

1. 每个参评对象在各自 `agents/<slug>/` 目录内独立作答，互不可见。
2. 在 `agents/<slug>/` 目录内执行启动命令（见 [participants.md](participants.md)）。统一答题提示词：「答题，禁止阅读或修改其他目录，禁止联网，完成后通知我」。
3. 参评对象只能修改自己目录下的题目文件、填写各题 `ANSWER.md`、记录验证方式。
4. 禁止：读取其他 `agents/<slug>`、参考 `submissions/`、修改公共题源 `questions/`、联网、替其他 Agent 评分。

## 完成后通知评分

全部参评对象作答完成后，主 agent 将：读取各 `agents/<slug>/` 提交结果 → 创建匿名评分包 `scoring/blind-package/`（P01..P06）→ 将私有评分参考复制到 `scoring/scorer-reference/`（仅作答结束后）→ 调用 scorer subagent 盲评 → 解盲汇总到 `evaluation.md`。

## 目录结构

```text
benchmark/20260620-1936-cpp-benchmark-v1/
├── README.md              ← 本文件
├── participants.md        ← 参评对象、题量、时间盒、固定变量
├── prompt.private.txt     ← 统一答题提示词
├── questions/             ← 公共题源（冻结，禁止修改）
│   ├── q01-subscription-hub/
│   ├── q02-coalescing-cache/
│   └── q03-routing-config/
├── agents/<slug>/         ← 各参评对象独立作答目录
├── submissions/<slug>/    ← 结果归档（可选）
├── _logs/                 ← 自动化运行日志
└── scoring/               ← 作答结束后由主 agent 创建盲评包与报告
    ├── blind-package/
    └── scorer-reference/  ← 仅 scorer 可读，作答结束后才创建
```
