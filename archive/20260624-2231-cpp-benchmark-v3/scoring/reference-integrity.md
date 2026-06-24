# 评分参考完整性记录

> 本文件记录复制到 `scoring/scorer-reference/` 的私有评分参考的 SHA-256，供复核。复制发生在全部 6 个参评对象作答结束并停止之后。

| 文件 | SHA-256 | 源（仓库内私有） |
|------|---------|------------------|
| `scorer-reference/benchmark-v3.md` | `fb5b0fbf71c66bf04c2921235afbc8aeb6c83b0e867042a083de9ac4a6fab0a7` | `.claude/skills/agent-benchmark/scoring-reference/benchmark-v3.md` |
| `scorer-reference/q01_truth.json` | `9f74e668fb3a67f529be02d9e87b6801ca998b06b0f8337fc2baa3d6bd89c3aa` | `.claude/skills/agent-benchmark/presets/benchmark-v3/q01-batch-engine-architecture/_private/q01_truth.json` |

机器评分脚本 `grade_svg.py`（未复制到 scorer-reference，由主 agent 在评分前运行）产出的 `scoring/machine-grade.json` 已一并交给匿名 scorer，作为"正确性"维度的客观依据。
