# 匿名 ID → 参评对象映射（仅主 agent，不给 scorer）

| 匿名 ID | 参评对象 | slug | 模型 |
|---------|----------|------|------|
| P01 | opencode + coding-deepseek | opencode-coding-deepseek | xag/coding-deepseek |
| P02 | opencode + coding-glm | opencode-coding-glm | xag/coding-glm |
| P03 | opencode + coding-kimi | opencode-coding-kimi | xag/coding-kimi |
| P04 | opencode + coding-minimax | opencode-coding-minimax | xag/coding-minimax |
| P05 | opencode + coding-qwen | opencode-coding-qwen | xag/coding-qwen |
| P06 | opencode + coding-qwen-preview | opencode-coding-qwen-preview | xag/coding-qwen-preview |

> 顺序按 slug 字母序。scorer subagent 只看到 P01..P06，不看到此映射。
