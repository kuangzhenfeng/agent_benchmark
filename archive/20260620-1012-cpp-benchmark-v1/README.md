# Agent Benchmark Run — 20260620-1012-cpp-benchmark

## 概述

本轮评测 5 个 AI Coding Agent × 模型组合在 C++17 工程任务上的表现。采用**同题试题包 → 独立目录作答 → 匿名 subagent 盲评 → 解盲汇总**的流程，避免评分方知道参评身份导致偏差。

- **run-id**：`20260620-1012-cpp-benchmark`
- **预设**：`cpp17-advanced-v1`（公共题库，本轮内不得修改）
- **题量 / 难度**：3 道 C++17 高级工程题（Bugfix / Implementation / Refactor）
- **生成日期**：2026-06-20

## 参评对象

详见 [participants.md](participants.md)。

## 目录结构

```text
benchmark/20260620-1012-cpp-benchmark/
├── README.md                 ← 本文件
├── participants.md           ← 参评对象、固定变量、时间盒
├── questions/                ← 公共题源（所有 Agent 同题）
│   ├── q01-subscription-hub/
│   ├── q02-coalescing-cache/
│   └── q03-routing-config/
├── agents/                   ← 每个参评对象的独立作答目录
│   ├── claude-code-qwen3-7-max/
│   ├── opencode-qwen3-7-max/
│   ├── claude-code-glm-5-2/
│   ├── opencode-glm-5-2/
│   └── codex-gpt-5-5/
├── submissions/              ← 用户可选的结果归档目录
└── scoring/                  ← 全部作答结束后才使用
    ├── blind-package/        ← 匿名评分包（P01/P02…）
    └── scorer-reference/     ← 私有参考解答，仅供 scorer subagent
```

## 题目与时间盒

| 题号 | 类型 | 时间盒 | 评测重点 |
|------|------|--------|----------|
| Q1 `subscription-hub` | 组合 Bugfix | 50 分钟 | 所有权、回调重入、异常安全、32 位时钟回绕、并发批次语义 |
| Q2 `coalescing-cache` | Implementation | 70 分钟 | 并发状态机、请求合并、TTL、失效竞态、LRU、递归加载 |
| Q3 `routing-config` | Refactor/Design | 60 分钟 | 快照发布、兼容 API 生命周期、观察者隔离、原子 reload、最长前缀匹配 |

## 如何分发与作答

1. 分别把每个待评测 Agent 指向它自己的 `agents/<slug>/` 目录。
2. 每个 Agent 先读该目录下的 `README.md`，再在对应的 `qXX-*/` 内答题。
3. Agent 只能修改自己目录内的文件，不得读取其他 `agents/<slug>`、`submissions/` 或公共题源 `questions/`。
4. 每题需填写 `ANSWER.md`（修改说明、验证方式、风险）。
5. 每题总时间盒如上表；超时按未完成项记录。

## 公开检查的初始状态（非预评分）

以下状态描述的是 **starter 代码**的客观事实，用于帮助参评 Agent 和后续 scorer 理解公开检查覆盖范围，**不是对任何参评对象的预评分**：

- **Q1**：基础路径可通过；完整公开套件含已文档化的 callable 生命周期/重入/批次边界检查，starter 状态预期失败。
- **Q2**：starter 为未实现骨架，完整公开套件预期失败。
- **Q3**：基础路径可通过；完整公开套件含跨实例 `current()` 生命周期、observer 复制重入检查，starter 状态预期失败。

> 说明：Q1 的 callable 生命周期锁边界是该预设中唯一标记为「硬性封顶条件」的语义；其余公开语义按正确性 / C++ 质量 / 约束遵循 / 验证证据评分，不因单项失败自动触发分数上限。

## 环境固定变量

- 编译器：`g++ 9.4.0`（Ubuntu 20.04，默认 `-std=c++17`），`clang++ 10.0.0` 可用作交叉验证
- 题目自包含，不依赖网络、私有 SDK 或外部服务

## 完成后如何通知评分

全部 Agent 停止作答后，告诉主 agent：

> 已完成，结果在 20260620-1012-cpp-benchmark

主 agent 将读取各提交结果，构造匿名评分包（`P01/P02…`），由 scorer subagent 盲评，最后解盲汇总到 `evaluation.md`。

## 公平性说明

- 所有 Agent 拿到完全相同的题目、starter code、公开测试与作答指令。
- 参考解答不进入题源或作答目录；只在全部作答结束后复制到 `scoring/scorer-reference/`，仅供匿名 scorer。
- scorer subagent 只看到匿名 ID，不知道 `Pxx` 对应哪个 Agent 或模型。
