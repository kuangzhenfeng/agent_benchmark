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

## 本轮作答结果

- 已完成 `q01-long-context-protocol/src/range_validator.cpp` 与 `q01-long-context-protocol/ANSWER.md`
- 已完成 `q02-protocol-adaptation/src/protocol_adapter.cpp` 与 `q02-protocol-adaptation/ANSWER.md`
- 验证结果：
  - `q01-long-context-protocol/./run_public_checks.sh` 通过
  - `q02-protocol-adaptation/./run_public_checks.sh` 通过
  - 额外脚本将源码规则表与题面文档抽取真值全量对表，Q1/Q2 均为 `0 mismatch`
- 约束执行：
  - 只读取并修改了当前目录内容
  - 未联网
  - 未同步当前目录外的 `score.md`
