# 评分参考完整性记录

私有评分参考与 ground truth 仅在全部 5 个参评对象停止作答后，由主 agent 复制到 `scoring/scorer-reference/`。来源为仓库版本化预设，未含参评身份、评分结论或任何参评提交内容。

| 文件 | SHA-256 | 源路径（预设内） |
|------|---------|------------------|
| `cpp17-advanced-v2.md` | `264a124c91c0277a6c4409cf120df51549316a9549db7e58f1212e2c4e74e4ed` | `.claude/skills/agent-benchmark/scoring-reference/cpp17-advanced-v2.md` |
| `q01_truth.json` | `4c3dc0279b1f10c5a665c17d4456911a912a13514712feed4c286e7e3cbc8f05` | `presets/cpp17-advanced-v2/q01-long-context-protocol/_private/q01_truth.json` |
| `q02_truth.json` | `69249f101f1cadb08c0f58dfdaea44c9b6afb2f12433f23d6416fbaaab95f3f8` | `presets/cpp17-advanced-v2/q02-protocol-adaptation/_private/q02_truth.json` |

复制前后 SHA-256 一致。该副本仅供匿名 scorer 读取，不分发给参评对象，不进入 `blind-package/Pxx`。
