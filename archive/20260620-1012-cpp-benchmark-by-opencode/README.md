# Benchmark Scoring — 20260620-1012-cpp-benchmark-by-opencode

评分报告，由 opencode 对 `archive/20260620-1012-cpp-benchmark` 的匿名盲评包进行双 scorer subagent 独立评分后生成。

## 文件说明

| 文件 | 说明 |
|------|------|
| `evaluation.md` | 解盲后的最终评测报告（含排名、分题评分、关键差异） |
| `scoring/scorer-a-report.md` | Scorer A 的完整匿名盲评报告 |
| `scoring/scorer-b-report.md` | Scorer B 的完整匿名盲评报告 |

## 评分方法

- 采用两个独立 scorer subagent 盲评
- 差异调和：取均值 + Q3 硬性封顶误用修正
- 解盲映射见 `evaluation.md` 中的解盲映射表

## 来源

- 盲评包来源：`archive/20260620-1012-cpp-benchmark/scoring/blind-package/`
- 映射文件：`archive/20260620-1012-cpp-benchmark/scoring/mapping.private.md`
- 评分参考：`.claude/skills/agent-benchmark/scoring-reference/cpp17-advanced-v1.md`
