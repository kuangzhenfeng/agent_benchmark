# 评测 Agent 与自动化评测指令

本文件记录可参评的 AI Coding Agent + 模型组合，以及对应的自动化评测启动指令。在 `/agent-benchmark` 流程第 1 步确认参评对象时直接选用。

| 参评对象 | 实际启动命令 |
|----------|--------------|
| claude code + glm-5.2 | `claude -p <prompt> --model glm-5.2` |
| claude code + qwen3.7-max | `claude -p <prompt> --model coding-qwen-preview` |
| codex + gpt-5.5 | `codex exec <prompt> -m gpt-5.5 -s workspace-write` |
| opencode + glm-5.2 | `opencode run <prompt> -m stevenk.cn/glm-5.2` |
| opencode + qwen3.7-max | `opencode run <prompt> -m xag/coding-qwen-preview` |

## 说明

- `<prompt>` 统一替换为相同的答题提示词："答题，禁止阅读或修改其他目录，禁止联网，完成后通知我"。
- **目录 slug** 按 skill 规则由参评对象转成小写短横线目录名（如 `claude code + glm-5.2` → `claude-code-glm-5-2`），用作 `benchmark/<run-id>/agents/<slug>/`。
- **启动命令** 在各参评对象的作答目录（`agents/<slug>/`）内执行，使该 Agent 只能看到分配给自己的题目，满足目录隔离要求。
- 多个组合可同时参评；分别在其各自目录内运行对应命令即可。
