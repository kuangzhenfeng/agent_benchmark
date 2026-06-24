# 盲评评分说明（scorer 必读）

你是 C++ benchmark 的盲评评分员。你**只知道**匿名包里是 P01、P02、P03、P04、P05、P06 六个匿名参评对象。你**不知道也不应推断**它们分别对应哪个 Agent 或模型。

> **本轮追加盲评**：`P06` 为后续追加的参评对象提交，`P01`–`P05` 已归档裁决、分数不再变。若本轮只评 `P06`，只输出 `P06` 即可，不要重新评 `P01`–`P05`。

## 你只能读取的目录

- 本 blind-package 目录（`P01`..`P06`，每个含两题完整产物）
- 评分参考目录：`scoring/scorer-reference/`（含 `benchmark-v2.md`、`q01_truth.json`、`q02_truth.json`）

**禁止**读取：`mapping.private.md`、`participants.md`、`agents/`、`submissions/`、`questions/`、benchmark 目录之外任何与身份相关的文件。不要尝试推断身份。

## 题目与真值规模

| 题 | 任务 | 真值规模 | 公开检查覆盖 |
|----|------|----------|--------------|
| Q1 `q01-long-context-protocol` | 实现 range.tc v6 规则表 | 48 结构 × 10 字段 = 480 真值点 | 仅 8 结构 + flag_active |
| Q2 `q02-protocol-adaptation` | 系统 A→B 适配，实现系统 B 规则表 | 48 绑定结构 × 10 字段 | 仅 8 结构 |

公开检查通过（退出码 0）**不等于**正确——它只抽样 8 个结构。你必须用 `scorer-reference/q01_truth.json`（Q1 直接顶层）和 `q02_truth.json` 的 `system_B`（Q2）**逐字段核对全部 48×N 真值点**，统计正确率作为正确性核心依据。

### 字段映射（truth JSON 字段 ↔ 规则表 10 维）

规则表每行结构体的字段顺序一般为：`route_code, schema_rev, id_policy, replay_scope, stamp_required, ordinal_required, mode_flag, phase_value, branch, band(=min/max 或 min,max)`。

- Q1 truth 字段：`route_code, schema_rev, id_policy, replay_scope, stamp_required, ordinal_required, mode_flag, phase_value, branch, band[min,max]`（10 字段）。
- Q2 truth 字段（system_B）：`route_code, schema_rev, id_policy, replay_scope, stamp_required, ordinal_required, mode_flag, phase_value, branch, band_min, band_max`（注意额外有 `phase_idx`/`branch_idx`，但规则表 10 维对应的是 `phase_value`/`branch`/`band_min`/`band_max`，按提交源码实际字段名核对等价值）。

以 `benchmark-v2.md` 与各题 `QUESTION.md` 为准；ground truth JSON 用于校准。**接受任何行为等价的实现**：字段顺序、枚举命名、数组 vs pair 表示 band 等差异不扣分，只看语义值是否正确。

## rubric（每题 100 分）

| 维度 | 分值 | 标准 |
|------|------|------|
| 正确性 | 45 | 按 truth JSON 逐字段核对全部 48×N 真值点的正确率线性给分（正确率 ×45，四舍五入）；核心行为与边界（flag_active 逐位、accepts/replayable 语义）正确 |
| C++ 质量 | 20 | 合理使用 C++17、RAII、const、类型边界、错误处理、无 UB |
| 约束遵循 | 15 | 只改允许文件、未改公共头文件/测试/题源、未为非绑定结构加规则（Q2）、未改 corpus |
| 可维护性 | 10 | 简洁清晰、职责明确、无过度封装或无关重写 |
| 验证证据 | 10 | ANSWER.md 是否填写、是否说明真值来源与陷阱处理、是否记录验证命令 |

## 硬性扣分（按 SKILL.md）

| 情况 | 处理 |
|------|------|
| 未提交核心代码 | 该题总分 ≤ 30 |
| 编造测试/命令/不存在的文件 | 该题总分 ≤ 40 |
| 读取或引用其他参评对象结果 | 该题总分 ≤ 50 |
| 违反 QUESTION.md 中"硬性封顶条件"（如改 corpus/公开 API/测试、Q2 为非绑定结构加规则、机械复制系统 A 值、仅据单一来源填 Q1） | 该题总分 ≤ 60 |
| 大量无关重写导致无法审查 | 可维护性 0，正确性按可验证部分给分 |

## 评分输出格式

对每个匿名 ID（P01..P06）每题，输出一行汇总 + 简述依据，**必须引用源码事实与 truth JSON 核对结果**。建议格式：

```
### P01
- Q1: 正确性 4x/45 | C++质量 x/20 | 约束 x/15 | 可维护 x/10 | 验证 x/10 | 总分 xx
  - 真值核对：480 点中正确 N 点（正确率 R%）；错例：[结构.字段=提交值(应=truth值)] ...
  - ANSWER.md：[填写/未填/质量]
  - 约束：[是否只改允许文件等]
  - 依据：...
- Q2: ...（同上，48×N 点）
```

末尾给出每个匿名 ID 的 Q1+Q2 总分及排名（按总分）。**只用 P01..P06**，绝不猜测身份。若本轮仅追加评 `P06`，只输出 `P06` 的 Q1/Q2 单题分与依据即可。
