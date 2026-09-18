---
name: agent-benchmark
description: 维护单一 HTML/SVG 创意题，按 evaluation-agents.md 分发多 Agent 作答，并将所有结果合并为一个无需评分的静态展示网页。
---

# 多 Agent HTML / SVG 产物展示

当前题源固定为 [svg-animation / otter-apple-peeler-v1](presets/MANIFEST.md)。任务是创建一个单文件 HTML，用内联 SVG 绘制水獭操作手摇苹果削皮机的 2D 动画，并将题意与成品合并展示。

## 工作方式

- 维护题库与流程时直接推进；用户要求启动评测但未指定对象时，默认选择 `evaluation-agents.md`“已登记组合”中的 ALL，不再询问。
- 题库仅保留 `presets/otter-apple-peeler/QUESTION.md` 一题，不划分难度。
- 给参评 Agent 的提示词必须原样采用 `QUESTION.md`，不得附加实现细节、验收清单或设计建议。
- 使用 `scripts/create_run.py` 从 `evaluation-agents.md` 生成 `benchmark/<run-id>/`、隔离参评副本和统一启动命令；生成分发不等于授权自动启动。
- 每个参评对象将结果写入自己的 `agents/<slug>/showcase/index.html`。
- 全部完成后使用 `scripts/build_showcase.py` 合并到仓库根目录 `showcase/index.html`，并保留轮次内 `showcase.html`。
- 不创建私有参考、匿名包、评分报告、排名或成绩。
- 变更题面或展示页时，同步更新 `presets/MANIFEST.md`、`presets/manifest.json`、仓库 `README.md` 与 `score.md` 的当前状态。

## 给 Agent 的提示词边界

提示词只包含用户指定的任务原句，以及交付路径、单文件离线运行这两项必要约束。成品展示页可以具备更丰富的交互，但不得将这些实现反向写入题目，从而限制参评 Agent 的创作空间。

## 分发与汇总

```bash
# 不传选择参数时默认 ALL；显式 --all 等价
python3 .claude/skills/agent-benchmark/scripts/create_run.py

# 也可重复选择 Harness 与模型表中已登记的组合
python3 .claude/skills/agent-benchmark/scripts/create_run.py \
  --agent 'codex + gpt-5.5' \
  --agent 'omp + glm-5.2'

# 全部作答结束后合并展示
python3 .claude/skills/agent-benchmark/scripts/build_showcase.py --run <run-id>
```

`build_showcase.py` 必须保留每个 Agent 的原始 HTML，并通过沙箱 iframe 嵌入统一页面；缺失结果显示占位状态，不能伪装为已完成。

## 验证

至少执行以下检查：

1. `manifest.json` 可解析，且只登记当前一题。
2. 无选择参数的 `create_run.py` 默认生成 ALL，重复 `--agent` 能覆盖默认选择；两种方式都生成隔离目录、`run.json` 和 `launch.md`，并拒绝覆盖已有轮次。
3. `build_showcase.py` 能正确汇总完成与缺失结果，生成单个展示入口且不改写原始提交。
4. 能使用浏览器时检查汇总页桌面与手机宽度及控制台错误；受环境策略阻止时如实说明，不伪造视觉验收结果。
