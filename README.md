# SVG 动画 Agent Benchmark

当前题库已重建为单一产物型任务：**创建一个 HTML，用 SVG 绘制一只水獭操作手摇苹果削皮机的 2D 动画。** 支持从 [evaluation-agents.md](evaluation-agents.md) 选择多个 Harness + 模型组合并行作答，最终把所有结果合并到一个网页；不进行评分、排名或匿名盲评。

## 当前题目

| 题目 | 交付要求 | 最终展示 |
|---|---|---|
| [水獭的苹果削皮工坊](.claude/skills/agent-benchmark/presets/otter-apple-peeler/QUESTION.md) | 单文件 HTML、内联 SVG、机械联动动画、响应式与减少动态支持 | [打开展示页](showcase/index.html) |

题目清单与机器可读约束分别见 [MANIFEST.md](.claude/skills/agent-benchmark/presets/MANIFEST.md) 和 [manifest.json](.claude/skills/agent-benchmark/presets/manifest.json)。

给参评 Agent 的提示词刻意保持精简：仅包含用户指定的任务原句，以及交付路径与单文件离线运行约束；展示页现有的交互与视觉实现不属于题面要求。

## 创建多 Agent 轮次

未指定参评对象时默认使用 ALL，即 `evaluation-agents.md`“已登记组合”中的全部对象；只有传入 `--agent` 时才覆盖该默认选择。

```bash
# 默认分发给全部已登记组合，不自动启动
python3 .claude/skills/agent-benchmark/scripts/create_run.py

# 或选择一个/多个已登记 Harness + 模型
python3 .claude/skills/agent-benchmark/scripts/create_run.py \
  --agent 'codex + gpt-5.5' \
  --agent 'omp + glm-5.2'
```

生成的 `benchmark/<run-id>/` 包含公共题面、每个对象的隔离目录、`participants.md`、`run.json` 与 `launch.md`。`launch.md` 使用各对象自己的 `QUESTION.md` 原文作为完整提示词；脚本不会自动执行 Agent。

## 合并展示结果

所有对象完成后运行：

```bash
python3 .claude/skills/agent-benchmark/scripts/build_showcase.py --run <run-id>
```

脚本将每个 `agents/<slug>/showcase/index.html` 嵌入同一个 [展示页](showcase/index.html)，并在轮次目录保存 `showcase.html`。原始结果保持不变，缺失结果显示等待状态，各页面在隔离 iframe 中运行。

## 查看当前展示

直接用浏览器打开：

```bash
open showcase/index.html
```

展示页无需安装依赖或启动服务；未运行多 Agent 轮次时保留当前示例，汇总后替换为本轮多结果页面。

## 当前结构

```text
.
├── PRODUCT.md
├── DESIGN.md
├── evaluation-agents.md
├── showcase/
│   └── index.html
└── .claude/skills/agent-benchmark/presets/
    ├── MANIFEST.md
    ├── manifest.json
    └── otter-apple-peeler/
        └── QUESTION.md
```

多 Agent 分发与展示脚本位于 `.claude/skills/agent-benchmark/scripts/`。旧题对应的私有评分与盲评流程已移除，当前题源不会向 `score.md` 写入新成绩。
