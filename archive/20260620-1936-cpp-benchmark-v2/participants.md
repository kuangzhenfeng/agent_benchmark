# 参评对象与固定变量

## 参评对象

| 参评对象 | slug | 模型 (经 xag provider) | 启动命令（在 `agents/<slug>/` 内执行） |
|----------|------|------------------------|----------------------------------------|
| opencode + coding-deepseek | `opencode-coding-deepseek` | xag/coding-deepseek | `opencode run <prompt> -m xag/coding-deepseek` |
| opencode + coding-minimax | `opencode-coding-minimax` | xag/coding-minimax | `opencode run <prompt> -m xag/coding-minimax` |
| opencode + coding-glm | `opencode-coding-glm` | xag/coding-glm | `opencode run <prompt> -m xag/coding-glm` |
| opencode + coding-qwen | `opencode-coding-qwen` | xag/coding-qwen | `opencode run <prompt> -m xag/coding-qwen` |
| opencode + coding-qwen-preview | `opencode-coding-qwen-preview` | xag/coding-qwen-preview | `opencode run <prompt> -m xag/coding-qwen-preview` |
| opencode + coding-kimi | `opencode-coding-kimi` | xag/coding-kimi | `opencode run <prompt> -m xag/coding-kimi` |

统一答题提示词 `<prompt>`：「答题，禁止阅读或修改其他目录，禁止联网，完成后通知我」（写入 `prompt.private.txt`）。

## 固定变量（公平性控制）

| 变量 | 取值 | 说明 |
|------|------|------|
| 预设 | `benchmark-v2` | 6 个参评对象共用同一份题源副本 |
| 题量 | 2 | Q1 long-context-protocol、Q2 protocol-adaptation |
| 语言标准 | C++17 | `-std=c++17` |
| 编译选项 | `-Wall -Wextra -Wpedantic -pthread` | 见各题 `run_public_checks.sh` |
| 时间盒 | Q1 300 分钟、Q2 300 分钟 | 超时按未完成项记录 |
| 目录隔离 | 各自 `agents/<slug>/` | 互相不可见，题源 `questions/` 冻结 |
| 提示词 | 全部使用同一 `<prompt>` | 避免 prompt 质量差异 |
| Provider | 全部 `xag` | 统一接入端点 |
| 公开检查 | 各题 `run_public_checks.sh` | 同一脚本，相同判据 |

## 工具权限（记录潜在差异）

本轮全部为 opencode + 同类 coding 模型，Agent 本体能力一致；统一要求「禁止联网」。执行能力差异主要来自模型本身，记录为公平性限制，评分时在 `evaluation.md` 注明。

## 编译器版本（待记录）

实际评测时在各作答环境执行 `g++ --version` 记录版本，填入下表（评分时用于说明未固定环境）：

| slug | g++ 版本 |
|------|----------|
| opencode-coding-deepseek | （待填） |
| opencode-coding-minimax | （待填） |
| opencode-coding-glm | （待填） |
| opencode-coding-qwen | （待填） |
| opencode-coding-qwen-preview | （待填） |
| opencode-coding-kimi | （待填） |

## 预设题面公开检查初始状态

- Q1 long-context-protocol：starter 填未读全语料的错误直觉值 → 公开检查预期失败。
- Q2 protocol-adaptation：starter 为系统 A 迁移前值 → 公开检查预期失败。

以上为预设 starter 起点，非预评分。
