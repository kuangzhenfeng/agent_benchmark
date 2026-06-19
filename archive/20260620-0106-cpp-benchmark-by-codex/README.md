# 编码 Agent 基准评测 — 20260620-0106-cpp-benchmark

本轮对五个 AI 编码 Agent + 模型组合在 C++17 工程任务上的表现进行盲评。

- **预设**：`cpp17-advanced-v1`（3 题，默认不修改）
- **题量**：3 道（Q1 组合 Bugfix / Q2 Implementation / Q3 Refactor-Design）
- **时间盒**：Q1 50 分钟、Q2 70 分钟、Q3 60 分钟
- **编译器**：`g++ (Ubuntu 9.4.0) 9.4.0`，`-std=c++17 -Wall -Wextra -Wpedantic -pthread`；环境另有 `clang++` 可用
- **参评对象**：见 `participants.md`（5 个组合）

## 目录职责

| 路径 | 用途 |
|------|------|
| `questions/` | 公共题源，所有参评对象同题，禁止修改 |
| `agents/<slug>/` | 单个参评对象独立作答目录 |
| `submissions/<slug>/` | 可选的结果归档目录 |
| `scoring/` | 作答结束后才创建的匿名评分包与私有参考解答 |

## 公开检查初始状态（starter 原样状态，不代表修复完成）

> 已在本轮生成时实跑确认，不构成对任何参评对象的预评分。

| 题目 | starter 公开检查 | 说明 |
|------|------------------|------|
| Q1 `subscription-hub` | ✅ 通过 | 基础冒烟路径通过，但题面描述的组合生产缺陷仍在，不视为修复完成 |
| Q2 `coalescing-cache` | ❌ 失败 | starter 仅提供可编译骨架，未实现，符合预期；完成实现后应通过 |
| Q3 `routing-config` | ✅ 通过 | 基础 reload 路径通过，但题面描述的半更新/悬空引用/重入问题仍在，不视为修复完成 |

## 作答分发

请把每个待评测 Agent 指向各自的作答目录：

- `claude code + qwen3.7max` → `agents/claude-code-qwen3-7-max/`
- `opencode + qwen3.7max` → `agents/opencode-qwen3-7-max/`
- `claude code + glm-5.2` → `agents/claude-code-glm-5-2/`
- `opencode + glm-5.2` → `agents/opencode-glm-5-2/`
- `codex + gpt-5.5` → `agents/codex-gpt-5-5/`

每个 `agents/<slug>/README.md` 写明了禁止事项和提交要求。

## 盲评状态

已于 2026-06-20 完成一次重新盲评：评分包已重新匿名化，参考解答已校验完整性，并由两位独立评分员评分和匿名复核。正式结果见 [evaluation.md](evaluation.md)；先前评分产物保留在 `scoring/archive-before-reblind-20260620/` 以便审计。

## 作答阶段说明（历史）

全部参评对象停止作答后，告诉我「已完成，结果在 `20260620-0106-cpp-benchmark`」。主 agent 会读取提交结果、创建匿名评分包、调用 scorer subagent 盲评，再解盲汇总到 `evaluation.md`。
