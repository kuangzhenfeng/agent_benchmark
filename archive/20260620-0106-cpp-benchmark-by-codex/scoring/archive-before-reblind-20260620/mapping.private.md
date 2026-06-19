# 匿名映射（私有，仅供主 agent，绝不传给 scorer subagent）

| 匿名 ID | 参评对象（用户写法） | slug |
|----------|----------------------|------|
| P01 | claude code + glm-5.2 | `claude-code-glm-5-2` |
| P02 | claude code + qwen3.7max | `claude-code-qwen3-7-max` |
| P03 | codex + gpt-5.5 | `codex-gpt-5-5` |
| P04 | opencode + glm-5.2 | `opencode-glm-5-2` |
| P05 | opencode + qwen3.7max | `opencode-qwen3-7-max` |

> 匿名 ID 按 slug 字母序分配，顺序与评分结果无关。
> 本文件只在主 agent 解盲汇总时使用，scorer subagent 只能看到 `blind-package/P01..P05`，看不到本文件。
