# 参评对象与固定变量

> 本轮 `benchmark-v3`（产物型架构图绘制）。题目：`q01-batch-engine-architecture`（1 题）。

## 参评对象

| # | 参评对象（原样保留） | slug | 启动命令 |
|---|----------------------|------|----------|
| 1 | claude code + glm-5.2 | `claude-code-glm-5-2` | `claude -p <prompt> --model glm-5.2` |
| 2 | claude code + qwen3.7-max | `claude-code-qwen3-7-max` | `claude -p <prompt> --model coding-qwen-preview` |
| 3 | codex + gpt-5.5 | `codex-gpt-5-5` | `codex exec <prompt> -m gpt-5.5 -s workspace-write` |
| 4 | codex + gpt-5.4 | `codex-gpt-5-4` | `codex exec <prompt> -m gpt-5.4 -s workspace-write` |
| 5 | opencode + glm-5.2 | `opencode-glm-5-2` | `opencode run <prompt> -m stevenk.cn/glm-5.2` |
| 6 | opencode + qwen3.7-max | `opencode-qwen3-7-max` | `opencode run <prompt> -m xag/coding-qwen-preview` |

`<prompt>` 统一为："答题，禁止阅读或修改其他目录，禁止联网，完成后通知我"。各命令在对应 `agents/<slug>/` 目录内执行。

## 固定变量（公平性控制）

| 变量 | 取值 |
|------|------|
| 预设 | `benchmark-v3`（题目版本冻结，不临时改题） |
| 题源 | 6 个作答目录均为同一题源副本（已逐一比对与 `questions/` 一致） |
| 时间盒 | 120–150 分钟（推荐，不限死；超时按未完成项记录） |
| 工具权限 | 联网：禁止；文件读写：仅本目录；命令执行：可（编译 demo + 跑 `run_public_checks.sh`）；测试：可自写脚本但不得联网 |
| 编译器 | g++ 9.4.0，`-std=c++17 -Wall -Wextra -Wpedantic -pthread` |
| Python | 3.10.16（`svg_check.py` 解析；评分用 `grade_svg.py`） |
| 环境 | 同一机器同一 shell；记录为已固定 |
| 提示词 | 所有参评对象用同一 `<prompt>` 与同一题面 |

## 未完全固定的变量（限制记录）

- 参评 Agent 各自的对话上下文/历史无法控制；
- 各 Agent 实际花费时间仅"推荐"，非硬性同时间盒；
- 产物型任务不跑测试套件，正确性以 `data-*` 标注与私有 ground truth 的集合覆盖率为客观依据，视觉风格（配色/布局）主观，不计入正确性。

## 本题评测重点

读懂可运行 C++17 批处理引擎 `TaskForge` 后产出 5 种风格 SVG 架构图（分层/组件依赖/数据流/类关系/部署）；命名近似与死代码辨别、运行时回调 vs 编译期依赖、接口依赖 vs 具体类依赖、注释误导抵抗；SVG `data-*` 标注可机器核对。

## 私密性

本轮 `questions/` 与 `agents/*/` 不含任何私有材料（`_private/`、`_generator/`、`machine-grade.json`、`q01_truth.json`、`grade_svg.py`、`verify_truth.py` 均已排除，实测确认）。私有评分参考只在全部作答结束后进入 `scoring/scorer-reference/`。
