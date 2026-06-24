# Agent Benchmark · 20260620-1936-cpp-benchmark-v2

本轮评测 **opencode + 6 个 coding 模型**在长上下文 C++ 协议任务上的效果。预设：`benchmark-v2`。

## 总览

| 项 | 内容 |
|----|------|
| 预设 | `benchmark-v2` |
| 题量 | 2 题（Q1 long-context-protocol、Q2 protocol-adaptation） |
| 语言标准 | C++17 |
| 编译器 | g++（记录实际版本见 participants.md） |
| 时间盒 | Q1 300 分钟、Q2 300 分钟（每题独立计时，自动化运行以墙钟上限控制） |
| 参评对象数 | 6 |
| Provider | 全部经 `xag`（https://tokens.xa.com/v1） |
| 公开检查 | 各题目录内 `run_public_checks.sh` |

## 题目

| 题号 | 短名 | 类型 | 推荐耗时 | 评测重点 |
|------|------|------|----------|----------|
| Q1 | long-context-protocol | 长上下文实现 | 300 分钟 | ~0.8M token 航天测控协议演进语料、纵向版本长程记忆、跨距离交叉引用、文档优先级取舍、幻觉抵抗 |
| Q2 | protocol-adaptation | 协议适配 | 300 分钟 | 两套 proto 物模型、横向相似系统逐字段迁移、近似命名辨别、enum 重编号、bit flag、范围控制 |

## 公开检查初始状态（预设声明，非预评分）

- **Q1 `long-context-protocol`**：starter 填的是未读全语料的错误直觉值，公开检查**预期失败**。
- **Q2 `protocol-adaptation`**：starter 是系统 A 迁移前值，公开检查**预期失败**。

> 该声明仅说明 starter 起点，不代表对任何参评对象的预评分。参评对象需通过阅读 `corpus/`（Q1）与 `系统A/`、`系统B/`（Q2）并据规则取舍后修正，使公开检查通过。

## 如何分发与作答

1. 每个参评对象在各自 `agents/<slug>/` 目录内独立作答，互不可见。
2. 在 `agents/<slug>/` 目录内执行启动命令（见 [participants.md](participants.md)）。统一答题提示词：「答题，禁止阅读或修改其他目录，禁止联网，完成后通知我」。
3. 参评对象只能修改自己目录下的题目文件、填写各题 `ANSWER.md`、记录验证方式。
4. 禁止：读取其他 `agents/<slug>`、参考 `submissions/`、修改公共题源 `questions/`、联网、替其他 Agent 评分。

## 完成后通知评分

全部参评对象作答完成后，主 agent 将：读取各 `agents/<slug>/` 提交结果 → 创建匿名评分包 `scoring/blind-package/`（P01..P06）→ 将私有评分参考与 ground truth 复制到 `scoring/scorer-reference/`（仅作答结束后）→ 调用 scorer subagent 盲评 → 解盲汇总到 `evaluation.md`。

## 目录结构

```text
benchmark/20260620-1936-cpp-benchmark-v2/
├── README.md              ← 本文件
├── participants.md        ← 参评对象、题量、时间盒、固定变量
├── prompt.private.txt     ← 统一答题提示词
├── questions/             ← 公共题源（冻结，禁止修改）
│   ├── q01-long-context-protocol/   (含 corpus/ ~1.9M)
│   └── q02-protocol-adaptation/     (含 系统A/ 系统B/)
├── agents/<slug>/         ← 各参评对象独立作答目录
├── submissions/<slug>/    ← 结果归档（可选）
├── _logs/                 ← 自动化运行日志
└── scoring/               ← 作答结束后由主 agent 创建盲评包与报告
    ├── blind-package/
    └── scorer-reference/  ← 仅 scorer 可读，作答结束后才创建
```
