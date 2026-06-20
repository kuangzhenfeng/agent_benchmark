# C++ benchmark v2 盲评报告（评分员 A）

参评对象：P01..P06（匿名）。rubric 见 `blind-package/README.md`：每题 100 分（正确性 45 / C++ 质量 20 / 约束 15 / 可维护 10 / 验证 10），两题满分 200。

客观正确率统一取自 `scorer-reference/machine-grade.json`（主 agent 逐字段核对结果）。公开检查退出码由本评分员在 `Pxx/qXX-*/` 目录实测 `bash run_public_checks.sh` 得到（g++ 9.4.0，C++17，`-Wall -Wextra -Wpedantic`）。所有 6 份提交的 `include/*.hpp`、`tests/public_checks.cpp`、`run_public_checks.sh`、`corpus/adr/ADR-009.md` 的 md5 完全一致（公共头/测试/语料均未被改动）。

关键事实（用于硬性扣分判断）：

| P | q01 src 状态 | q02 src 状态 |
|---|---|---|
| P01 | 已重写为 v6 真值（machine-grade 480/480） | 已迁移到系统 B（528/528） |
| P02 | 已重写为 v6 真值（480/480） | 已迁移到系统 B（528/528） |
| P03 | 与 starter 字节一致（`stage_state: 230,5,auto_gen,none,...0x04`），未作答核心代码 | 与 starter（系统 A 值）字节一致，未迁移 |
| P04 | 与 starter 字节一致（与 P03 q01 同 md5 `1a8d4b88...`） | 与 starter 字节一致（与 P03 q02 同 md5 `a0b58950...`） |
| P05 | 与 starter 字节一致（与 P03/P04 q01 同 md5） | 已迁移到系统 B（528/528） |
| P06 | 已重写为 v6 真值（472/480，8 字段错） | 已迁移到系统 B（528/528） |

公开检查退出码实测：

| P | q01 | q02 |
|---|---|---|
| P01 | 0 (pass) | 0 (pass) |
| P02 | 0 (pass) | 0 (pass) |
| P03 | 134 (assert fail) | 134 (assert fail) |
| P04 | 134 (assert fail) | 134 (assert fail) |
| P05 | 134 (assert fail) | 0 (pass) |
| P06 | 0 (pass) | 0 (pass) |

---

### P01

- Q1 long-context-protocol: 正确性 45/45 | C++ 质量 20/20 | 约束 15/15 | 可维护 10/10 | 验证 10/10 | **总分 100**
  - machine-grade：480 点中正确 480（正确率 100%），`q1_errors` 为空。`stage_state.route_code=576`、`thrust_report.route_code=385`、`thrust_report.stamp_required=false` 等 ADR-009 高优先级（勘误推翻 proto）字段全部命中。
  - 源码：`src/range_validator.cpp` 仅重写 `kRules`，派生函数 `accepts/replayable/flag_active/phase_rank/branch_of/within_band` 全部由 `rule_for` 驱动，逐位掩码 `(mode_flag & (1u<<bit))`、`within_band` 双侧闭区间，const 引用遍历，无 UB；编译 0 警告（`-Wall -Wextra -Wpedantic`）。顶部注释明确写出 ADR-009 优先级与 v6 阶段编号。
  - 约束：只改 `src/range_validator.cpp`；`include`/`tests`/`corpus` md5 与他者一致，未触碰公共头/测试/语料。
  - ANSWER.md：已填写，列出 ADR-009 优先级、勘误覆盖示例（ERR-07/09/11/02/04）、changelog 链式追溯、验证命令与抽样结构；逐字段来源层有交代（按勘误/changelog/proto 三层）。可验证证据充分。
  - 公开检查：pass（exit 0）。
  - 依据：正确率满分 + 代码规范 + 约束零违反 + ANSWER 完整 → 满分。

- Q2 protocol-adaptation: 正确性 45/45 | C++ 质量 20/20 | 约束 15/15 | 可维护 10/10 | 验证 10/10 | **总分 100**
  - machine-grade：528 点中正确 528（正确率 100%），`q2_errors` 为空。`stage_state: 765,keep_blank,vehicle,...phase=3,config` 等系统 B 真值全部命中，与系统 A（201,auto_gen,stage,phase=0,command）不同，确认非机械复制。
  - 源码：`src/protocol_adapter.cpp` 仅重写 `kRules`；`accepts` 注释明确"仅 strict 拒绝空标识"，stamp/ordinal 独立；`flag_active` 逐位；`phase_rank` 返回系统 B 编号（reserved=4/coast=5/insertion=6）；编译 0 警告。
  - 约束：只改 `src/protocol_adapter.cpp`；未为非绑定结构加规则（48 项 = 全部绑定）；未改公开头/测试/系统A/系统B/变更映射。
  - ANSWER.md：已填写，点明"从系统 B proto 注释逐结构独立读取"、阶段编号差异、不复制系统 A 值、不加 trim/哈希；给出验证命令与抽样结构。
  - 公开检查：pass（exit 0）。
  - 依据：正确率满分 + 横向迁移干净 + 防误映射说明到位 → 满分。

**P01 总分：200 / 200**

---

### P02

- Q1 long-context-protocol: 正确性 45/45 | C++ 质量 20/20 | 约束 15/15 | 可维护 10/10 | 验证 10/10 | **总分 100**
  - machine-grade：480 点中正确 480（正确率 100%），`q1_errors` 为空，与 P01 同等正确率。
  - 源码：`src/range_validator.cpp` 仅重写 `kRules`（值与 P01 完全一致），派生函数全部由 `rule_for` 驱动，逐位掩码、闭区间、const 正确，0 警告。
  - 约束：只改 `src`；公共头/测试/语料 md5 一致。
  - ANSWER.md：已填写且质量最高——逐类陷阱处理（勘误推翻 proto、词典"已废弃"实为重新启用、ms→0.1ms 单位漂移、v5 枚举重编号、历史代码/测试过时值、近似变体混用）各举实例；明确复述 ADR-009 优先级。逐字段来源层（proto/changelog/errata 三层）交代清晰。
  - 公开检查：pass（exit 0）。
  - 依据：正确率满分 + 陷阱处理说明最完整 → 满分。

- Q2 protocol-adaptation: 正确性 45/45 | C++ 质量 20/20 | 约束 15/15 | 可维护 10/10 | 验证 10/10 | **总分 100**
  - machine-grade：528 点中正确 528（正确率 100%），`q2_errors` 为空。
  - 源码：`src/protocol_adapter.cpp` 仅重写 `kRules`；`accepts` 注释、逐位 `flag_active`、系统 B 阶段编号；0 警告。规则值与 P01/P05/P06 一致，确认从系统 B 独立读取而非复制系统 A。
  - 约束：只改 `src/protocol_adapter.cpp`；48 项均为绑定结构；未改公共工件。
  - ANSWER.md：已填写，给出 A→B 差异举例（stage_state route 201→765、phase 0→3、branch command→config）、阶段编号差异、变体名称独立读取、派生函数语义；验证命令齐全。
  - 公开检查：pass（exit 0）。
  - 依据：正确率满分 + 迁移防误映射说明具体 → 满分。

**P02 总分：200 / 200**

---

### P03

- Q1 long-context-protocol: 正确性 23/45 | C++ 质量 18/20 | 约束 15/15 | 可维护 4/10 | 验证 0/10 | **总分 60 → 硬性封顶 ≤30（未提交核心代码）→ 30**
  - machine-grade：480 点中正确 259（正确率 53.96%），即 `round(0.5396×45)=24`，取 23–24。但 `src/range_validator.cpp` 与 starter **字节一致**（md5 `1a8d4b8805b0dc39de6e62289538a30a`，注释仍是"未读全语料的错误直觉值"，`stage_state: 230,5,auto_gen,none,...0x04` 即 starter 错误直觉值）→ 触发硬性规则"未修改 src（与 starter 一致）= 未作答核心代码 → 该题 ≤ 30"。
  - 公开检查：fail（exit 134，`public_checks.cpp:11` 断言 `r->route_code == exp.route_code` 失败，starter 错误值被抽样断言命中）。
  - ANSWER.md：**未填写**，仍是题目模板原文（"逐结构（48 个）列出每个字段的 v6 真值…"占位文字），无任何来源层/陷阱处理/验证记录 → 验证证据 0。
  - 源码 C++ 质量本身合法（starter 代码无 UB、const 正确、C++17、0 警告），故 C++ 质量给 18；但因未实际作答，可维护性（无可审查的校正工作）给 4。
  - 依据：未作答核心代码，硬性封顶 30。

- Q2 protocol-adaptation: 正确性 11/45 | C++ 质量 18/20 | 约束 15/15 | 可维护 4/10 | 验证 0/10 | **总分 → 硬性封顶 ≤30 → 30**
  - machine-grade：528 点中正确 129（正确率 24.43%），`round(0.2443×45)=11`。
  - 源码：`src/protocol_adapter.cpp` 与 starter（系统 A 迁移前值）**字节一致**（md5 `a0b589503939976d89313ce82f776848`，注释仍是"当前为【系统 A 的已完成值】(迁移前)"，`stage_state: 201,5,auto_gen,stage,...0x2c,0,command` 即系统 A 值）→ 未迁移，硬性 ≤30。
  - 公开检查：fail（exit 134）。
  - ANSWER.md：未填写，仍是模板原文 → 验证 0。
  - 依据：未迁移核心代码，硬性封顶 30。

**P03 总分：60 / 200**

---

### P04

- Q1 long-context-protocol: 正确性 23/45 | C++ 质量 18/20 | 约束 15/15 | 可维护 4/10 | 验证 0/10 | **总分 → 硬性封顶 ≤30 → 30**
  - machine-grade：480 点中正确 259（53.96%），`q1_errors` 与 P03 逐条相同。
  - 源码：`src/range_validator.cpp` 与 P03 q01 **字节一致**（同 md5 `1a8d4b88...`，starter 未改）→ 未作答核心代码，硬性 ≤30。
  - 公开检查：fail（exit 134）。
  - ANSWER.md：未填写，模板原文 → 验证 0。
  - 依据：与 P03 q1 完全同构，未作答，硬性封顶 30。

- Q2 protocol-adaptation: 正确性 11/45 | C++ 质量 18/20 | 约束 15/15 | 可维护 4/10 | 验证 0/10 | **总分 → 硬性封顶 ≤30 → 30**
  - machine-grade：528 点中正确 129（24.43%），`q2_errors` 与 P03 逐条相同。
  - 源码：`src/protocol_adapter.cpp` 与 P03 q02 **字节一致**（同 md5 `a0b58950...`，系统 A starter 未迁移）→ 硬性 ≤30。
  - 公开检查：fail（exit 134）。
  - ANSWER.md：未填写，模板原文 → 验证 0。
  - 依据：未迁移，硬性封顶 30。

**P04 总分：60 / 200**

---

### P05

- Q1 long-context-protocol: 正确性 23/45 | C++ 质量 18/20 | 约束 15/15 | 可维护 4/10 | 验证 0/10 | **总分 → 硬性封顶 ≤30 → 30**
  - machine-grade：480 点中正确 259（53.96%），`q1_errors` 与 P03/P04 逐条相同。
  - 源码：`src/range_validator.cpp` 与 starter **字节一致**（同 md5 `1a8d4b88...`，`stage_state: 230,5,auto_gen,none,0x04` starter 值未改）→ 未作答 Q1 核心代码，硬性 ≤30。
  - 公开检查：fail（exit 134）。
  - ANSWER.md：未填写（Q1 的 ANSWER.md 仍是模板）→ 验证 0。
  - 依据：Q1 未作答，硬性封顶 30。

- Q2 protocol-adaptation: 正确性 45/45 | C++ 质量 20/20 | 约束 15/15 | 可维护 10/10 | 验证 8/10 | **总分 98**
  - machine-grade：528 点中正确 528（正确率 100%），`q2_errors` 为空。`stage_state: 765,keep_blank,vehicle,phase=3,config` 等系统 B 真值全部命中，与系统 A 值不同，非机械复制。
  - 源码：`src/protocol_adapter.cpp` 已重写 `kRules`（md5 `14afb8e1...`，与 starter 不同）；注释写明"从系统B proto文件注释中提取, 逐结构独立读取, 未从系统 A 复制"；`accepts` 注释、逐位 `flag_active`、系统 B 阶段编号；0 警告。
  - 约束：只改 `src/protocol_adapter.cpp`；48 项均为绑定结构；公共工件 md5 一致。
  - ANSWER.md：Q2 的 ANSWER.md 仍是**模板原文**（未填写适配摘要/防误映射/派生函数说明/验证），仅源码本身正确。验证证据维度因 ANSWER 未填写而扣 2（代码本身可编译且公开检查 pass 提供了部分客观证据，故不全扣）。
  - 公开检查：pass（exit 0）。
  - 依据：正确率满分 + 代码迁移干净，但 ANSWER.md 未填写 → 验证维度扣 2。

**P05 总分：128 / 200**

---

### P06

- Q1 long-context-protocol: 正确性 44/45 | C++ 质量 20/20 | 约束 15/15 | 可维护 10/10 | 验证 10/10 | **总分 99**
  - machine-grade：480 点中正确 472（正确率 98.33%），`round(0.9833×45)=44`。8 处错误集中在 4 个结构：`thrust_report_view.id_policy`（keep_blank 应=strict）、`nav_solution_patch.replay_scope`（none 应=stage）、`nav_solution_v2.stamp_required`（false 应=true）、`fts_status.schema_rev`（3 应=6）、`fts_status_legacy`（schema_rev 3 应=6、stamp_required true 应=false、ordinal_required true 应=false、phase_value 2 应=1）。均为个别字段漏改/勘误遗漏，非系统性的"仅据单一来源"（其余 472 字段已按 ADR-009 优先级正确取舍），故不触发"仅据单一来源→≤60"硬性封顶。
  - 源码：`src/range_validator.cpp` 已重写 `kRules`（md5 `6cabae0f...`），派生函数全部由 `rule_for` 驱动，逐位掩码、闭区间、const 正确，0 警告。`stage_state.route_code=576`、`thrust_report.route_code=385` 等高优先级勘误字段已正确命中。
  - 约束：只改 `src`；公共头/测试/语料 md5 一致。
  - ANSWER.md：已填写，方法步骤清晰（ADR-009 优先级、勘误叠加覆盖）、单位漂移/枚举重编号/近似名称陷阱各举例、验证命令与抽样结构齐全；逐字段来源层有交代。
  - 公开检查：pass（exit 0）。
  - 依据：正确率 98.33%（-1）+ 其余维度满分 → 99。

- Q2 protocol-adaptation: 正确性 45/45 | C++ 质量 20/20 | 约束 15/15 | 可维护 10/10 | 验证 10/10 | **总分 100**
  - machine-grade：528 点中正确 528（正确率 100%），`q2_errors` 为空。
  - 源码：`src/protocol_adapter.cpp` 已重写 `kRules`（md5 `6c825f1a...`），`stage_state: 765,keep_blank,vehicle,phase=3,config` 等系统 B 真值全部命中，与系统 A 值不同；派生函数全部由 `rule_for` 驱动，0 警告。
  - 约束：只改 `src/protocol_adapter.cpp`；48 项均为绑定结构；公共工件未改。
  - ANSWER.md：已填写，方法（变更映射定位 + 系统 B proto 逐字段独立读取）、防误映射（不复刻系统 A、抵抗近似名称、系统 B 阶段编号）、派生函数语义、验证命令齐全。
  - 公开检查：pass（exit 0）。
  - 依据：正确率满分 + 迁移干净 + 说明完整 → 满分。

**P06 总分：199 / 200**

---

## 汇总与排名（200 满）

| 排名 | 匿名 ID | Q1 | Q2 | 总分 |
|------|---------|-----|-----|------|
| 1 | P01 | 100 | 100 | **200** |
| 1 | P02 | 100 | 100 | **200** |
| 3 | P06 | 99 | 100 | **199** |
| 4 | P05 | 30 | 98 | **128** |
| 5 | P03 | 30 | 30 | **60** |
| 5 | P04 | 30 | 30 | **60** |

说明：

- P01 / P02 两题全满分，正确率均为 100%，C++ 质量、约束、可维护、验证均满分；P02 的 Q1 ANSWER 陷阱处理叙述最完整。
- P06 Q1 仅有 8 字段（4 结构）漏改（98.33%），Q2 满分，总分 199，紧随其后。
- P05 只完成了 Q2 迁移（满分级别），Q1 源码与 ANSWER 均未动（starter 未改），Q1 硬性封顶 30。
- P03 / P04 两题源码均与 starter 字节一致（未作答核心代码），ANSWER 均为模板未填，两题各硬性封顶 30。

---

评分员 A，未读取任何身份信息。
