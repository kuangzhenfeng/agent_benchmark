---
name: agent-benchmark
description: 维护单一的 HTML/SVG 创意产物题，将题目与结果合并为一个无需评分的静态展示网页。
---

# HTML / SVG 产物展示

当前题源固定为 [svg-animation / otter-apple-peeler-v1](presets/MANIFEST.md)。任务是创建一个单文件 HTML，用内联 SVG 绘制水獭操作手摇苹果削皮机的 2D 动画，并将题意与成品合并展示。

## 工作方式

- 维护题库时直接推进，不询问参评对象。
- 题库仅保留 `presets/otter-apple-peeler/QUESTION.md` 一题，不划分难度。
- 给参评 Agent 的提示词必须原样采用 `QUESTION.md`，不得附加实现细节、验收清单或设计建议。
- 最终交付物固定为仓库根目录 `showcase/index.html`。
- 不创建 `benchmark/` 轮次、参评副本、私有参考、匿名包、评分报告或成绩。
- 变更题面或展示页时，同步更新 `presets/MANIFEST.md`、`presets/manifest.json`、仓库 `README.md` 与 `score.md` 的当前状态。

## 给 Agent 的提示词边界

提示词只包含用户指定的任务原句，以及交付路径、单文件离线运行这两项必要约束。成品展示页可以具备更丰富的交互，但不得将这些实现反向写入题目，从而限制参评 Agent 的创作空间。

## 验证

至少执行以下检查：

1. `manifest.json` 可解析，且只登记当前一题。
2. HTML 可被解析，且只包含一个内联 SVG 和一个内联脚本。
3. 页面包含动画控制、减少动态媒体查询与必要的 SVG 角色/机械结构。
4. 能使用浏览器时检查桌面与手机宽度、交互状态及控制台错误；受环境策略阻止时如实说明，不伪造视觉验收结果。
