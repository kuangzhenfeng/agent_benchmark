# 匿名映射（私密，仅供主 agent，绝不传给 scorer subagent）

| 匿名 ID | 参评对象（Agent + 模型） | slug | 完成 exit | SVG 公开检查 |
|---------|--------------------------|------|-----------|--------------|
| P01 | claude code + glm-5.2 | claude-code-glm-5-2 | 0 | PASS |
| P02 | codex + gpt-5.5 | codex-gpt-5-5 | 0 | PASS |
| P03 | opencode + qwen3.7-max | opencode-qwen3-7-max | 0 | PASS |
| P04 | claude code + qwen3.7-max | claude-code-qwen3-7-max | 0 | PASS |
| P05 | codex + gpt-5.4 | codex-gpt-5-4 | 0 | PASS |
| P06 | opencode + glm-5.2 | opencode-glm-5-2 | 0 | PASS |

匿名 ID 顺序为人工分配，与真实能力排序无关。全部 6 个作答进程在 22:56:32 同时启动，最迟 23:11:36 结束，均在 150 分钟硬上限内（实际 ~15 分钟内全部完成）。

机器评分（`machine-grade.json`，覆盖率 /45，正确性维度客观依据）：

| 匿名 ID | 覆盖率 raw | trap 加分 | 合计/45 | interface_deps 陷阱 |
|---------|-----------|-----------|---------|---------------------|
| P01 | 21.68 | +2.0 | 23.68 | 未命中 |
| P02 | 28.24 | +2.0 | 30.24 | 未命中 |
| P03 | 17.94 | +2.0 | 19.94 | 未命中 |
| P04 | 24.42 | +3.0 | 27.42 | **命中** |
| P05 | 28.39 | +2.0 | 30.39 | 未命中 |
| P06 | 23.08 | +2.0 | 25.08 | 未命中 |
