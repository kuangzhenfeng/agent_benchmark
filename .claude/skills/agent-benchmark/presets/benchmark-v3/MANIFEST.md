# benchmark-v3

面向「读懂既有 C++ 代码库 → 用 SVG 可视化架构」能力的基准预设。与 v1（并发/所有权工程实践）、v2（长上下文/协议适配）不同，本预设是**产物型**任务：给一套真实可编译运行的 C++17 批处理引擎 `TaskForge`，要求参评 Agent 通读源码后产出 **5 种风格的 SVG 架构图**，再匿名盲评。

| 题目 | 类型 | 推荐时间盒 | 评测重点 |
|------|------|------------|----------|
| q01-batch-engine-architecture | 架构图绘制（产物型） | 120–150 分钟 | 架构理解与抽象表达、分层/依赖/数据流/类关系/部署五视角、命名近似与死代码辨别、运行时回调 vs 编译期依赖、接口依赖 vs 具体类依赖、注释误导抵抗 |

## 公开性与边界

- 所有架构事实都能从公开源码（`include/` + `src/`）推导；评分不依赖未说明的"隐藏谜题"。
- 5 张 SVG 的 ground truth 由私有 `_private/q01_truth.json` 枚举（5 视角的节点/边/分层/陷阱正确判断），由人工据源码编写，并经 `_generator/verify_truth.py` 校验每个真值点可由源码证明、引擎可编译运行。该文件严禁进入题源或作答目录。
- 正确性的客观核对依赖参评对象在 SVG 中按约定嵌入的 `data-*` 标注（`data-style` / `data-role="nodes|edges"` / `data-id` / `data-from`+`data-to`+`data-kind`）。视觉风格（配色/布局/装饰）完全自由，只锁语义结构层。这是本预设把"画图正确性"从主观压成客观覆盖率的核心机制。
- 评分期主 agent 运行 `_generator/grade_svg.py`，解析各匿名提交的 SVG 标注并与 ground truth 做集合匹配，产出 `machine-grade.json`（5 视角节点/边 P/R/F1 + 陷阱命中标志），供匿名 scorer 直接引用"正确性"维度。
- 本预设不保证任何模型或人类必然失败或通过。能力结论只能来自同题、隔离作答与匿名盲评。

## 重新校验题源

题源为手写源码 + 手写 ground truth，无需"生成"。开发/维护时可运行自洽校验：

```bash
cd _generator
python3 verify_truth.py ..   # 引擎编译运行 + truth 每节点/边/陷阱可由源码证
# 评分脚本自测（用 truth 派生一份样例 SVG 跑 grade_svg.py，应得高覆盖率）
python3 grade_svg.py ../q01-batch-engine-architecture/_private/q01_truth.json <样例diagrams> --single
```

## 使用规则

- 用户明确选择 `benchmark-v3` 时，完整复制题目目录 `q01-batch-engine-architecture` 到本轮 `questions`，再复制到每个 `agents/<slug>/`。复制时**不得**带上 `_generator/` 与 `_private/` 目录——这些是私有生成器与 ground truth，严禁进入题源或作答目录。
- `include/`、`src/`、`QUESTION.md`、`ANSWER.md`、`svg_check.py`、`run_public_checks.sh`、`diagrams/`（占位 SVG）都是公开题源；所有参评对象必须拿到字节等价副本。
- 参评对象只产出/修改 `diagrams/*.svg` 与 `ANSWER.md`，不得修改引擎源码、`QUESTION.md`、`svg_check.py` 或 `_private/`。
- 公开检查 `run_public_checks.sh`：引擎 demo 编译运行 **PASS**（证明源真实、暴露运行时给参评对象观测），但 5 张占位 SVG 缺 `data-*` 标注使 `svg_check.py` **FAIL**——这是预期状态，不是对任何参评对象的预评分。
- 全部参评对象停止作答后，主 agent 才把私有评分参考（`scoring-reference/benchmark-v3.md` + `_private/q01_truth.json`）复制到本轮 `scoring/scorer-reference/`，并运行 `grade_svg.py` 产出 `machine-grade.json` 一并交给匿名 scorer。
