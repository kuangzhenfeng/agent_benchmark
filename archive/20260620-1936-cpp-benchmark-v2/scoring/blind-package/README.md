# 盲评评分说明（scorer 必读）

你是 C++ benchmark 的盲评评分员。你**只知道**匿名包里是 P01、P02、P03、P04、P05、P06 六个匿名参评对象。你**不知道也不应推断**它们分别对应哪个 Agent 或模型。

## 你只能读取的目录

- 本 blind-package 目录（`P01`..`P06`，每个含两题完整产物：`QUESTION.md`、`ANSWER.md`、`corpus/`（Q1）、`系统A/`、`系统B/`（Q2）、`include/`、`src/`、`tests/`、`run_public_checks.sh`）
- 评分参考目录：`scoring/scorer-reference/`（含 `cpp17-advanced-v2.md`、`q01_truth.json`、`q02_truth.json`、`machine-grade.json`）

**禁止**读取：`mapping.private.md`、`participants.md`、`agents/`、`submissions/`、`questions/`、benchmark 目录之外任何与身份相关的文件。不要尝试推断身份。

## 题目（预设 cpp17-advanced-v2，2 题，每题 100 分）

| 题 | 任务 | 真值规模 | 公开检查覆盖 |
|----|------|----------|--------------|
| Q1 `q01-long-context-protocol` | 实现 range.tc v6 规则表（48 结构 × 10 维） | 480 真值点 | 仅抽样 8 结构 + flag_active |
| Q2 `q02-protocol-adaptation` | 系统 A→B 适配，实现系统 B 规则表（48 结构 × 11 维） | 528 真值点 | 仅抽样 8 结构 |

公开检查通过（退出码 0）**不等于**正确——它只抽样 8 个结构。你必须用 `scorer-reference/machine-grade.json`（主 agent 确定性脚本对每提交逐字段核对的客观结果）与 `q01_truth.json` / `q02_truth.json` 的 ground truth 核对正确率，作为正确性核心依据。`machine-grade.json` 已是匿名结果，直接引用即可。

### 字段映射（truth JSON ↔ 规则表 10 维）

- Q1 truth：`route_code, schema_rev, id_policy, replay_scope, stamp_required, ordinal_required, mode_flag, phase_value, branch, band[min,max]`（10 维）。
- Q2 truth（system_B）：`route_code, schema_rev, id_policy, replay_scope, stamp_required, ordinal_required, mode_flag, phase_value, branch, band_min, band_max`（11 维，另含 `phase_idx/branch_idx` 派生索引不计入 10 维核对）。

以 `cpp17-advanced-v2.md` 与各题 `QUESTION.md` 为准；ground truth JSON 用于校准。**接受任何行为等价的实现**：字段顺序、枚举命名、数组 vs pair 表示 band 等差异不扣分，只看语义值是否正确。

## 如何评分

对每个匿名对象（P01..P06）每题：

1. **读 `machine-grade.json`**：取该 Pxx 的 `q1_correct/q1_total`、`q2_correct/q2_total`（主 agent 已逐字段比对）。正确率 = 正确性核心依据。
2. **读题面** `QUESTION.md`（Q1 重点 ADR-009 优先级取舍与幻觉抵抗；Q2 重点横向迁移、避免机械复制系统 A）。
3. **读源码** `src/*.cpp`，确认派生函数（accepts/replayable/flag_active/phase_rank/branch_of/within_band）是否由 rule_for 驱动、有无 UB、const 正确、C++17。
4. **读 `ANSWER.md`**：是否填写了真值来源层（逐字段 provenance）、陷阱处理（errata 推翻/单位漂移/枚举重编号等）、迁移防误映射。
5. **运行公开检查**：在 `Pxx/qXX-*/` 目录 `bash run_public_checks.sh`，记录退出码（0=通过，134=断言失败）。注意：**未修改 src 的提交（与 starter 字节一致）= 未作答核心代码**，公开检查会因 starter 错误值断言失败（134）。

## rubric（每题 100 分）

| 维度 | 分值 | 标准 |
|------|------|------|
| 正确性 | 45 | 按 machine-grade.json 正确率线性给分（正确率 ×45，四舍五入）；核心行为与边界正确 |
| C++ 质量 | 20 | 合理使用 C++17、RAII、const、类型边界、无 UB、派生函数由 rule_for 驱动 |
| 约束遵循 | 15 | 只改允许文件、未改公共头/corpus/系统A/B、未为非绑定结构加规则（Q2）、C++17 |
| 可维护性 | 10 | 简洁清晰、职责明确、无过度封装或无关重写 |
| 验证证据 | 10 | ANSWER.md 是否填写、是否说明真值来源层与陷阱处理、是否记录验证命令 |

## 硬性扣分（按 SKILL.md）

| 情况 | 处理 |
|------|------|
| 未提交核心代码（src 与 starter 一致 / 编译不过 / 无规则表） | 该题总分 ≤ 30 |
| 编造测试/命令/不存在的文件 | 该题总分 ≤ 40 |
| 读取或引用其他参评对象结果 | 该题总分 ≤ 50 |
| 违反 QUESTION.md 中"硬性封顶条件"（改 corpus/公开 API/测试、Q2 为非绑定结构加规则、机械复制系统 A 值、仅据单一来源填 Q1） | 该题总分 ≤ 60 |
| 大量无关重写导致无法审查 | 可维护性 0，正确性按可验证部分给分 |

## 评分输出格式

对每个匿名 ID（P01..P06）每题，输出：

```
### P0X
- Q1 long-context-protocol: 正确性 x/45 | C++质量 x/20 | 约束 x/15 | 可维护 x/10 | 验证 x/10 | 总分 xx
  - machine-grade：480 点中正确 N（正确率 R%）；错例：[结构.字段=提交值(应=truth值)] ...
  - ANSWER.md：[填写/未填/质量，是否逐字段来源层]
  - 公开检查：[pass / fail(134) / 未改src]
  - 依据：...
- Q2 protocol-adaptation: ...（528 点，是否机械复制系统 A）
```

末尾给出每个匿名 ID 的 Q1+Q2 总分（200 满）及排名（按总分）。**只用 P01..P06**，绝不猜测身份。
