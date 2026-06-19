# Agent Benchmark

> 用“参评对象确认 → 同题试题包生成 → 独立目录作答 → 匿名 subagent 盲评 → 解盲汇总报告”的流程，公平评测多个 AI Coding Agent / 模型组合在 **C++ 编码任务**上的效果。

本项目核心是一个 Claude Code Skill：[`/.claude/skills/agent-benchmark/SKILL.md`](./.claude/skills/agent-benchmark/SKILL.md)。运行流程、公平性控制、评分 rubric、报告模板全部由该 skill 定义，本 README 仅作快速入口与目录约定说明。

## 它解决什么问题

评测编码 Agent 时最容易出现的偏差是：**评分方知道参评身份**，从而对熟悉的 Agent/模型产生期待偏差。本项目通过以下机制规避：

- **同题同源**：所有参评对象拿到完全相同的 C++ 题目与公开验收点；
- **目录隔离**：每个参评对象只在自己的 `agents/<slug>/` 下作答，互不可见；
- **不泄答案**：生成阶段只写题面、starter code 和公开验收，不写参考答案；
- **匿名盲评**：评分交给独立 scorer subagent，只能看到匿名包里的 `P01/P02`，看不到 Agent 或模型名称；
- **证据评分**：scorer 基于源码、题面、验证记录和统一 rubric 打分，不只看 `ANSWER.md` 自述；
- **克制结论**：只对本轮题型和样本范围下结论，不外推到所有任务。

## 快速开始

在 Claude Code 中直接触发 skill，或描述需求即可（如“评测一下 claude code + qwen 和 opencode + glm 在 C++ 任务上的表现”）。

```
/agent-benchmark
```

第一步 skill 一定会询问本次参评的 **Agent + 模型组合**（例如 `claude code + qwen3.7 max`、`opencode + glm-5.2`）。也可以顺带提供题量、难度、时间盒；不提供则默认 **3 道 C++17 题**，每题 20~40 分钟，覆盖 bugfix / implementation / refactor-design 三类。

## 完整流程

| 阶段 | 主 agent 动作 | 是否交给用户 |
|------|--------------|--------------|
| 1. 确认参评对象 | 询问 Agent + 模型组合，整理参评表 | 是，等用户回复 |
| 2. 设计试题 | 生成自包含、可静态评分的 C++17 题 | — |
| 3. 创建 benchmark 目录 | 在 `benchmarks/agent-benchmark/<run-id>/` 生成题源 + 各参评对象独立作答目录 | 生成后停止 |
| 4. 等待作答 | 不替参评 Agent 作答、不提前评分 | 是，用户分别让各 Agent 在各自目录答题 |
| 5. 匿名盲评 | 创建匿名评分包，调用 scorer subagent 按 rubric 盲评 | — |
| 6. 解盲汇总 | 映射匿名 ID 回参评对象，写 `evaluation.md` 并对话摘要 | — |

> 默认单 scorer 盲评；高敏感或分差很小的评测会起两个独立 scorer，差异超阈值才在匿名状态下复核。

## 评分维度（每题 100 分）

| 维度 | 分值 | 关注点 |
|------|------|--------|
| 正确性 | 45 | 是否满足题面要求，核心行为与边界条件 |
| C++ 质量 | 20 | C++17、RAII、const、类型边界、错误处理 |
| 约束遵循 | 15 | 目录隔离、题面限制、提交格式、禁止事项 |
| 可维护性 | 10 | 简洁清晰、职责明确、无无关重写 |
| 验证证据 | 10 | 可信测试、命令输出、推理或人工验证 |

另有硬性扣分规则（未提交核心代码、编造测试、读取他人结果、破坏题目结构等），详见 skill。

## 结论阈值

| 平均分差 | 写法 |
|----------|------|
| ≥ 15 且多数题胜出 | “本轮明显领先” |
| 8 ~ 15 | “存在优势，建议补测确认稳定性” |
| < 8 | “本轮未拉开显著差距” |

## 目录约定

```text
benchmarks/agent-benchmark/<run-id>/        # <run-id> 如 20260619-1430-cpp-benchmark
├── README.md                  # 本轮入口：如何分发、作答、完成后如何通知评分
├── participants.md            # 参评对象、题量、时间盒、固定变量
├── evaluation.md              # 最终解盲汇总报告
├── questions/                 # 公共题源（确认所有 Agent 同题）
│   └── qXX-<short-name>/
│       ├── QUESTION.md
│       ├── ANSWER.md          # 供参评 Agent 填写的模板
│       ├── include/ src/ tests/
├── agents/<agent-model-slug>/ # 单个参评对象的独立作答目录（互不可见）
├── submissions/<slug>/        # 可选的结果归档目录
└── scoring/
    ├── blind-package/         # 用户完成后由主 agent 创建，给 scorer 盲评
    │   ├── README.md          # rubric、扣分规则、输出模板（不含身份）
    │   └── P01/ P02/ …
    ├── mapping.private.md     # 匿名 ID → 参评对象，只给主 agent，不给 scorer
    ├── redaction-log.md       # 记录脱敏位置与类型，不写参评身份
    └── scorer-report.md       # scorer subagent 输出的匿名评分报告
```

目录命名约定：

- **参评对象原样保留**用户写法，如 `claude code + qwen3.7 max`；
- **slug** 转成小写短横线目录名，如 `claude-code-qwen3-7-max`；
- `benchmarks/` 与生成的 benchmark 目录均纳入 git；`.scratch` 不用于存放正式评测结果（`.gitignore` 中已忽略 `benchmark-run` 等临时产物）。

## 仓库内容

| 路径 | 说明 |
|------|------|
| [`.claude/skills/agent-benchmark/SKILL.md`](./.claude/skills/agent-benchmark/SKILL.md) | **核心**：完整流程、公平性控制、rubric、报告模板 |
| [`.claude/skills/create-skill/SKILL.md`](./.claude/skills/create-skill/SKILL.md) | 创建新 skill 的辅助 skill |
| [`CLAUDE.md`](./CLAUDE.md) → [`AGENTS.md`](./AGENTS.md) | 项目规范（AGENTS.md 为符号链接） |

> 若需调整流程、评分标准或目录结构，直接修改 [`SKILL.md`](./.claude/skills/agent-benchmark/SKILL.md)，并同步回本 README。
