# Agent Benchmark 作答说明

你只允许在当前目录内答题。

## 禁止

- 读取其他 `agents/<slug>` 目录
- 读取或参考 `submissions/` 中其他参评对象结果
- 修改 benchmark 公共题源 `questions/`
- 联网查资料（题目自包含，所有信息都在当前题目的 `corpus/` 与 `QUESTION.md` 中）
- 替其他 Agent 评分或读取评分相关目录

## 要求

- 每题在对应 `qXX-*` 目录内修改或新增文件
- 填写每题 `ANSWER.md`
- 记录你执行过的验证方式；如未验证，说明原因
- 完成后停止，不要替其他 Agent 评分

## 本轮题目

- `q01-long-context-protocol`：长上下文实现，推荐时间盒 300 分钟
- `q02-protocol-adaptation`：协议适配，推荐时间盒 300 分钟

题源已随本目录分发，无需联网。请先阅读各题 `QUESTION.md` 与 `corpus/`（Q1）或 `系统A/`、`系统B/`（Q2）。

## 作答记录（[REDACTED]）

| 题目 | 修改范围 | 公开检查 | 说明文档 |
|------|---------|---------|---------|
| `q01-long-context-protocol` | `src/range_validator.cpp` 的 `kRules`（48 结构 × 10 维，按 ADR-009 优先级取舍） | 退出码 0（8 锚点 + `flag_active`） | `q01-long-context-protocol/ANSWER.md` |
| `q02-protocol-adaptation` | `src/protocol_adapter.cpp` 的 `kRules`（48 结构 × 10 维，逐字段取自系统 B 契约块，飞行阶段用系统 B 编号） | 退出码 0（8 锚点 + `flag_active`） | `q02-protocol-adaptation/ANSWER.md` |

两题均仅改动对应 `qXX-*/src/*.cpp` 的规则表，未改公开头文件、测试、题源或任何协议工件；派生函数保持由规则表驱动不变。
