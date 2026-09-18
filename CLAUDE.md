# 项目规范

## 规则

- 注释用中文。
- 变更后同步更新 `README.md`。
- `archive/` 中的正式评测结果需简洁同步到 `score.md`；当前 SVG 产物题不评分，因此不新增归档分数。

## Agent 规则

- 当前仅维护一题：创建单文件 HTML，用 SVG 绘制水獭操作手摇苹果削皮机的 2D 动画。
- 题面放在 `.claude/skills/agent-benchmark/presets/otter-apple-peeler/QUESTION.md`。
- 支持按 `evaluation-agents.md` 选择多个参评对象；启动评测且未指定对象时默认选择“已登记组合”中的 ALL，不再询问。
- 每个参评对象在隔离目录内作答，提示词原样使用 `QUESTION.md`，不得追加实现提示。
- 全部结果通过 `build_showcase.py` 合并到 `showcase/index.html`，同时保留各 Agent 原始 HTML。
- 不生成评分、排名、匿名包或私有参考。
