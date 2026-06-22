# Participants — 20260620-1012-cpp-benchmark

## 参评对象

| 参评对象（原样） | slug | 作答目录 | Agent 类别 | 模型 |
|------------------|------|----------|-----------|------|
| claude code + qwen3.7max | `claude-code-qwen3-7-max` | `agents/claude-code-qwen3-7-max/` | Claude Code | Qwen3.7 Max |
| opencode + qwen3.7max | `opencode-qwen3-7-max` | `agents/opencode-qwen3-7-max/` | OpenCode | Qwen3.7 Max |
| claude code + glm-5.2 | `claude-code-glm-5-2` | `agents/claude-code-glm-5-2/` | Claude Code | GLM-5.2 |
| opencode + glm-5.2 | `opencode-glm-5-2` | `agents/opencode-glm-5-2/` | OpenCode | GLM-5.2 |
| codex + gpt-5.5 | `codex-gpt-5-5` | `agents/codex-gpt-5-5/` | Codex | GPT-5.5 |

> 说明：本轮同时比较两类维度——Agent 实现（Claude Code / OpenCode / Codex）与模型（Qwen3.7 Max / GLM-5.2 / GPT-5.5）。由于同一模型挂在两个 Agent 下，解盲后可粗略观察 Agent 框架差异与模型差异，但 5 个样本不足以分离交互效应，结论仅限本轮题型与样本范围。

## 固定变量

| 变量 | 值 |
|------|-----|
| 预设 | `cpp17-advanced-v1`（冻结，本轮不得修改） |
| 题量 | 3 |
| 题型覆盖 | Bugfix / Implementation / Refactor-Design |
| Q1 时间盒 | 50 分钟 |
| Q2 时间盒 | 70 分钟 |
| Q3 时间盒 | 60 分钟 |
| 语言标准 | C++17 |
| 编译器 | `g++ 9.4.0`（Ubuntu 20.04） |
| 交叉验证编译器 | `clang++ 10.0.0` |
| 构建工具 | CMake 3.16.3 |
| 题目来源 | `.claude/skills/agent-benchmark/presets/cpp17-advanced-v1/`（完整复制到 `questions/` 与各 `agents/<slug>/`） |
| 网络依赖 | 无 |
| 私有环境依赖 | 无 |

## 权限与能力（由用户在作答阶段记录）

> 作答期间请各 Agent 在各自 `ANSWER.md` 或 README 中说明实际使用的工具权限。以下为待记录项，用于评估工具差异对成绩的影响：

| 能力 | 说明 |
|------|------|
| 联网搜索 | 是否启用（题目不依赖网络） |
| 文件读写 | 默认允许（限本目录） |
| 命令执行 | 编译、跑 `run_public_checks.sh` 等 |
| 跨目录读取 | **禁止**（不得读其他 `agents/<slug>`、`submissions/`、`questions/` 之外的私有参考材料） |

## 未固定 / 限制

- **工具权限差异**：不同 Agent 默认工具集不同（如某些支持联网、某些沙箱更严），可能影响成绩；以各 Agent 自述与验证记录为准。
- **样本量**：5 个，不足以分离「Agent 框架 × 模型」的交互效应。
- **执行环境**：均为本地 g++ 9.4.0，但各 Agent 实际运行时是否调用编译器、是否真正执行测试，取决于其工具权限与行为，需在验证证据维度核查。
