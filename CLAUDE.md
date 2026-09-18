# 项目规范

## 规则

- 注释用中文。
- 变更后同步更新 `README.md`。
- `archive/` 中的正式评测结果需简洁同步到 `score.md`；当前 SVG 产物题不评分，因此不新增归档分数。

## Agent 规则

- 当前仅维护一题：创建单文件 HTML，用 SVG 绘制水獭操作手摇苹果削皮机的 2D 动画。
- 题面放在 `.claude/skills/agent-benchmark/presets/otter-apple-peeler/QUESTION.md`。
- 最终结果统一写入 `showcase/index.html`，题意与成品合并在同一页面展示。
- 不启动参评 Agent，不生成评分、排名、匿名包或私有参考。
- 页面必须离线可用、响应式、可键盘操作，并尊重 `prefers-reduced-motion`。
