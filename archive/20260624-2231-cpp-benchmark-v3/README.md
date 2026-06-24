# benchmark / 20260624-2231 · cpp-benchmark-v3

> 产物型架构图绘制评测：用 **benchmark-v3** 预设的 TaskForge 批处理引擎，评测 6 个 Agent+模型组合"读懂既有 C++ 代码 → 用 SVG 可视化架构"的能力。1 道题，匿名盲评。

## 本轮概况

| 项 | 值 |
|----|----|
| 预设 | `benchmark-v3`（产物型） |
| 题量 | 1 题（`q01-batch-engine-architecture`） |
| 时间盒 | 120–150 分钟（推荐，不限死） |
| 编译器 | g++ 9.4.0（`-std=c++17 -Wall -Wextra -Wpedantic -pthread`） |
| Python | 3.10.16（`svg_check.py` / 评分脚本 `grade_svg.py`） |
| 参评对象数 | 6 |

## 参评对象与目录

| 参评对象 | 作答目录 |
|----------|----------|
| claude code + glm-5.2 | `agents/claude-code-glm-5-2/` |
| claude code + qwen3.7-max | `agents/claude-code-qwen3-7-max/` |
| codex + gpt-5.5 | `agents/codex-gpt-5-5/` |
| codex + gpt-5.4 | `agents/codex-gpt-5-4/` |
| opencode + glm-5.2 | `agents/opencode-glm-5-2/` |
| opencode + qwen3.7-max | `agents/opencode-qwen3-7-max/` |

详见 `participants.md`。

## 分发作答

题源在 `questions/q01-batch-engine-architecture/`（字节等价副本已复制到每个 `agents/<slug>/q01-batch-engine-architecture/`，复制后已逐一比对一致）。

各参评对象只允许在自己目录内作答。启动命令见仓库根 `evaluation-agents.md`（在各 `agents/<slug>/` 目录内执行，`<prompt>` 统一为"答题，禁止阅读或修改其他目录，禁止联网，完成后通知我"）。

## 公开检查初始状态（预设声明）

已在本仓库题源目录实测确认：

- ✅ 引擎 demo 编译运行 **PASS**（`submitted=5 succeeded=4 failed=1 retried=4`），证明源真实、暴露运行时供参评对象观测；
- ❌ 5 张占位 SVG 缺 `data-style` 标注，`svg_check.py` **FAIL**。

这是预期初始状态，不是对任何参评对象的预评分。参评对象的目标是把 5 张占位 SVG 替换为带 `data-*` 标注的真实架构图，使公开检查通过。

## 完成后通知评分

全部 6 个参评对象作答并确认停止后，告诉我"已完成，结果在 20260624-2231-cpp-benchmark-v3"。届时主 agent 会：

1. 在 `scoring/scorer-reference/` 放入私有参考（`benchmark-v3.md` + `q01_truth.json`），并记录 SHA-256 完整性；
2. 运行 `grade_svg.py` 解析每个匿名提交的 `diagrams/`，与 ground truth 匹配产出 `scoring/machine-grade.json`；
3. 创建匿名盲评包（`P01`–`P06`，身份脱敏为 `[REDACTED]`）；
4. 调用 scorer subagent 按 rubric 盲评，写 `scoring/scorer-report.md`；
5. 解盲汇总到 `evaluation.md`。

## 私密性说明

`questions/` 与 `agents/*/` 严禁包含 `_private/`、`_generator/`、`machine-grade.json`、`q01_truth.json`、`grade_svg.py`、`verify_truth.py`——已实测确认本轮目录干净。这些私有材料只在全部作答结束后进入 `scoring/scorer-reference/`，且仅供匿名 scorer 读取。
