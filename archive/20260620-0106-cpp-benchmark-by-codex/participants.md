# Participants

| 字段 | 值 |
|------|----|
| 预设 | `cpp17-advanced-v1`（默认，未修改） |
| 题量 | 3（Q1 / Q2 / Q3） |
| 难度 | cpp17-advanced-v1（高级工程实践） |
| 时间盒 | Q1 50 分钟、Q2 70 分钟、Q3 60 分钟 |
| 语言/标准 | C++17（`-std=c++17 -Wall -Wextra -Wpedantic -pthread`） |
| 编译器 | `g++ (Ubuntu 9.4.0) 9.4.0`；另可 `clang++` |
| 操作系统 | Linux 6.6（WSL2），x86_64 |

## 参评对象

| 参评对象（用户写法） | slug | 作答目录 |
|----------------------|------|----------|
| claude code + qwen3.7max | `claude-code-qwen3-7-max` | `agents/claude-code-qwen3-7-max/` |
| opencode + qwen3.7max | `opencode-qwen3-7-max` | `agents/opencode-qwen3-7-max/` |
| claude code + glm-5.2 | `claude-code-glm-5-2` | `agents/claude-code-glm-5-2/` |
| opencode + glm-5.2 | `opencode-glm-5-2` | `agents/opencode-glm-5-2/` |
| codex + gpt-5.5 | `codex-gpt-5-5` | `agents/codex-gpt-5-5/` |

## 固定变量

- **同题同源**：五个参评对象使用从同一预设复制出的完全相同的三题（新参评对象初始内容已通过 sha256 比对确认与预设一致）。
- **隔离**：每个参评对象仅可修改自己的 `agents/<slug>/`，互相隔离；公共题源 `questions/` 锁定不动。
- **提示词一致**：两个参评对象使用同一份 `agents/<slug>/README.md` 作答说明（仅 slug 路径不同）。

## 工具权限（参评 Agent 可用，影响成绩，作记录）

- 文件读写：可，仅限各自作答目录。
- 命令执行：可（含 `g++`/`clang++` 编译、运行公开检查）。
- 联网：题目自包含，不依赖网络；若参评 Agent 联网属环境差异，列为残余风险。
- 测试：可自行新增测试与运行验证。

## 已记录的限制 / 残余风险

- 仅本机单一编译器/OS 环境，未跨平台/跨编译器固定。
- 样本量 = 5 参评对象 × 3 题，结论不外推到所有任务或模型。
- 作答耗时由各 Agent 自行控制；未做机器级硬超时强制，超时按未完成项记录。
