# 参评对象与固定变量

## 参评对象

| 参评对象 | slug | 启动命令（在 `agents/<slug>/` 内执行） |
|----------|------|----------------------------------------|
| claude code + glm-5.2 | `claude-code-glm-5-2` | `claude -p <prompt> --model glm-5.2` |
| claude code + qwen3.7-max | `claude-code-qwen3-7-max` | `claude -p <prompt> --model coding-qwen-preview` |
| codex + gpt-5.5 | `codex-gpt-5-5` | `codex exec <prompt> -m gpt-5.5 -s workspace-write` |
| opencode + glm-5.2 | `opencode-glm-5-2` | `opencode run <prompt> -m stevenk.cn/glm-5.2` |
| opencode + qwen3.7-max | `opencode-qwen3-7-max` | `opencode run <prompt> -m xag/coding-qwen-preview` |

统一答题提示词 `<prompt>`：「答题，禁止阅读或修改其他目录，禁止联网，完成后通知我」。

## 固定变量（公平性控制）

| 变量 | 取值 | 说明 |
|------|------|------|
| 预设 | `benchmark-v2` | 5 个参评对象共用同一份题源副本 |
| 题量 | 2 | Q1 long-context-protocol、Q2 protocol-adaptation |
| 语言标准 | C++17 | `-std=c++17` |
| 编译选项 | `-Wall -Wextra -Wpedantic -pthread` | 见各题 `run_public_checks.sh` |
| 时间盒 | Q1 300 分钟、Q2 300 分钟 | 超时按未完成项记录 |
| 目录隔离 | 各自 `agents/<slug>/` | 互相不可见，题源 `questions/` 冻结 |
| 提示词 | 全部使用同一 `<prompt>` | 避免 prompt 质量差异 |
| 公开检查 | 各题 `run_public_checks.sh` | 同一脚本，相同判据 |

## 工具权限（记录潜在差异）

各参评对象 Agent 的联网、文件读写、命令执行、测试权限由其本体能力决定，本轮统一要求「禁止联网」。其余执行能力差异记录为公平性限制，评分时在 `evaluation.md` 注明。

## 编译器版本（待记录）

实际评测时请在各作答环境执行 `g++ --version` 记录版本，填入下表（评分时用于说明未固定环境）：

| slug | g++ 版本 |
|------|----------|
| claude-code-glm-5-2 | （待填） |
| claude-code-qwen3-7-max | （待填） |
| codex-gpt-5-5 | （待填） |
| opencode-glm-5-2 | （待填） |
| opencode-qwen3-7-max | （待填） |

## 预设题面公开检查初始状态

- Q1 starter 填未读全语料的错误直觉值 → 公开检查预期失败。
- Q2 starter 为系统 A 迁移前值 → 公开检查预期失败。

以上为预设 starter 起点，非预评分。
