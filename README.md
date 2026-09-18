# SVG 动画 Agent Benchmark

当前题库已重建为单一产物型任务：**创建一个 HTML，用 SVG 绘制一只水獭操作手摇苹果削皮机的 2D 动画。** 不再保留原有 C++ 题目，也不进行评分、排名或匿名盲评。

## 当前题目

| 题目 | 交付要求 | 最终展示 |
|---|---|---|
| [水獭的苹果削皮工坊](.claude/skills/agent-benchmark/presets/otter-apple-peeler/QUESTION.md) | 单文件 HTML、内联 SVG、机械联动动画、响应式与减少动态支持 | [打开展示页](showcase/index.html) |

题目清单与机器可读约束分别见 [MANIFEST.md](.claude/skills/agent-benchmark/presets/MANIFEST.md) 和 [manifest.json](.claude/skills/agent-benchmark/presets/manifest.json)。

给参评 Agent 的提示词刻意保持精简：仅包含用户指定的任务原句，以及交付路径与单文件离线运行约束；展示页现有的交互与视觉实现不属于题面要求。

## 查看结果

直接用浏览器打开：

```bash
open showcase/index.html
```

展示页把题目概念、动画结果和控制项合并在一个页面中，无需安装依赖或启动服务。页面提供播放/暂停、慢速和重播；系统开启“减少动态”时自动显示完整静态场景。

## 当前结构

```text
.
├── PRODUCT.md
├── DESIGN.md
├── showcase/
│   └── index.html
└── .claude/skills/agent-benchmark/presets/
    ├── MANIFEST.md
    ├── manifest.json
    └── otter-apple-peeler/
        └── QUESTION.md
```

旧题对应的分发、私有评分与盲评脚本已一并移除；当前仓库只维护题面和单页成品，不会向 `score.md` 写入新成绩。
