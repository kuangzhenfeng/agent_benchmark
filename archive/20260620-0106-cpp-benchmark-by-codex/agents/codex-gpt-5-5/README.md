# Agent Benchmark Instructions

你只允许在当前目录内答题。

> 本目录属于参评对象：codex + gpt-5.5（slug: `codex-gpt-5-5`）
> 这是 **盲评 benchmark**，你的提交会被匿名化后由独立 scorer 评分。请专注工程实现本身，不需要、也不应该在代码或文档里声明你的 Agent/模型身份。

## 题目

| 题号 | 目录 | 时间盒 | 类型 |
|------|------|--------|------|
| Q1 | `q01-subscription-hub/` | 50 分钟 | 组合 Bugfix |
| Q2 | `q02-coalescing-cache/` | 70 分钟 | Implementation |
| Q3 | `q03-routing-config/` | 60 分钟 | Refactor/Design |

每题目录内有 `QUESTION.md`（目标/背景/约束/验收）、starter code（`include/`、`src/`）、`tests/public_checks.cpp`、`ANSWER.md` 模板和 `run_public_checks.sh`。

## 禁止

- 读取其他 `agents/<slug>/` 目录（包括 `claude-code-qwen3-7-max`、`opencode-qwen3-7-max`、`claude-code-glm-5-2`、`opencode-glm-5-2`）。
- 读取或参考 `submissions/` 中其他参评对象结果。
- 修改 benchmark 公共题源 `questions/`。
- 在代码、注释或文档中写入你的 Agent 名 / 模型名 / 原始目录 slug（匿名评分时这些会被当作需脱敏的身份信息）。

## 要求

- 每题在对应 `qXX-*` 目录内修改或新增文件；不要改变公共 API 签名。
- 填写每题 `ANSWER.md`（修改摘要、验证方式、剩余风险）。
- 运行 `./run_public_checks.sh` 记录结果；自行补充能证明边界语义的测试。
- 记录你执行过的验证方式（命令与输出）；如未验证，说明原因。
- 完成后停止，不要替其他 Agent 评分。

## 关于公开检查的提醒

- Q1、Q3 的 starter 基础公开检查可通过，但**不代表**修复完成——题面描述的组合缺陷仍在，公开检查只覆盖正常路径。
- Q2 的 starter 未实现，公开检查在完成实现前**预期失败**。
- 仅通过公开检查不等于达标，评分以题面完整验收标准为准。

## 已完成实现与验证

| 题目 | 实现要点 | 额外边界检查 |
|---|---|---|
| Q1 | 所有权化事件/订阅数据、可重入的批次 drain、取消检查、异常隔离与模时钟过期 | 临时字符串、批次边界、取消、异常、回绕 |
| Q2 | 单 key in-flight 合并、递归检测、失效屏蔽回填、完成时 TTL 与 LRU | TTL 边界、递归、在途失效 |
| Q3 | 不可变快照发布、`current()` 引用保活、解锁观察者通知与异常计数 | 失败发布、引用跨 reload、重入观察者、最长前缀 |

已实际执行三题各自的 `./run_public_checks.sh`，以及每题 `tests/edge_checks.cpp` 的 C++17 编译运行；均通过。各题的具体命令、结果和剩余风险见对应目录的 `ANSWER.md`。
