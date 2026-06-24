# 盲评评分报告 B（benchmark-v2）

独立第二评分员。仅依据 `blind-package/` 与 `scorer-reference/` 两个目录。评分对象为匿名 P01..P06，不读取、不推断任何身份信息。

## 评分依据速览

- **machine-grade.json**（主 agent 逐字段确定性核对，匿名结果）：Q1 共 480 真值点，Q2 共 528 真值点。正确率 = 正确性核心依据。
- **md5 核验**（判定 src 是否与 starter 字节一致 → "未提交核心代码"硬性封顶 ≤30）：

| 对象 | Q1 range_validator.cpp | Q2 protocol_adapter.cpp | 判定 |
|------|------------------------|-------------------------|------|
| P01 | `fd0ef52f…`（已改） | `b5a84c03…`（已改） | 两题均已提交核心代码 |
| P02 | `d196f987…`（已改，与 P01 不同） | `47494cd6…`（已改，与 P01 不同） | 两题均已提交核心代码 |
| P03 | `1a8d4b88…`（**starter 原样**） | `a0b58950…`（**starter 系统 A 原样**） | 两题均未提交核心代码 |
| P04 | `1a8d4b88…`（**starter 原样**） | `a0b58950…`（**starter 系统 A 原样**） | 两题均未提交核心代码 |
| P05 | `1a8d4b88…`（**starter 原样**） | `14afb8e1…`（已改） | Q1 未提交，Q2 已提交 |
| P06 | `6cabae0f…`（已改） | `6c825f1a…`（已改） | 两题均已提交核心代码 |

starter Q1 源码内嵌注释"未读全语料的错误直觉值"（约 45% 真值点错误）；starter Q2 内嵌注释"系统 A 的已完成值（迁移前）"，即机械复制系统 A 值。

- **公开检查退出码**（`bash run_public_checks.sh`）：

| 对象 | Q1 | Q2 |
|------|----|----|
| P01 | 0（通过） | 0（通过） |
| P02 | 0（通过） | 0（通过） |
| P03 | 134（断言失败） | 134（断言失败） |
| P04 | 134（断言失败） | 134（断言失败） |
| P05 | 134（断言失败） | 0（通过） |
| P06 | 0（通过） | 0（通过） |

134 = starter 错误值触发断言，符合"未提交核心代码"预期。

## rubric（每题 100 分）

正确性 45（= 正确率 × 45，四舍五入）｜ C++ 质量 20 ｜ 约束遵循 15 ｜ 可维护性 10 ｜ 验证证据 10。

硬性扣分：src 与 starter 一致 / 编译不过 / 无规则表 → 该题总分 ≤ 30；机械复制系统 A 值 → ≤ 60（与"未提交"叠加取更严的 ≤30）；仅据单一来源填 Q1 → ≤ 60。

---

## P01

### Q1 long-context-protocol：正确性 45/45 ｜ C++质量 20/20 ｜ 约束 15/15 ｜ 可维护 10/10 ｜ 验证 10/10 ｜ **总分 100**

- **machine-grade**：480/480（正确率 1.0），无错误。48 结构 × 10 维全部命中。抽样核验：`stage_state {576,2,auto_gen,vehicle,false,true,0x01,2,command,191,219}` 与 q01_truth.json 完全一致；`thrust_report.route_code=385`（ERR-02 推翻 proto 973）、`fts_status.band=[308,432]`（ERR-02 推翻 proto [110,285]）、`fts_status.schema_rev=6` 等关键陷阱字段均正确。
- **源码**（`fd0ef52f`，已改）：`constexpr std::array<FieldRule,48> kRules` 表驱动；`rule_for` 线性查找返回 `std::optional`；`accepts` 仅 strict 拒绝空标识、stamp/ordinal 独立；`flag_active` 逐位 `(mode_flag & (1u<<bit))!=0`；`phase_rank/branch_of/within_band` 均由 `rule_for` 驱动；const 正确，无 UB，C++17。
- **ANSWER.md**：已填写。文档优先级（勘误 > changelog > v6.proto > 词典 > 历史代码）正确复述；举 ERR-07/09/11/02/04 陷阱实例；验证记录"PASSED"。
- **公开检查**：退出码 0。
- **依据**：全维度满分。正确率 1.0×45=45；派生函数全部 rule_for 驱动、bitwise flag_active、const 正确 → C++满；仅改 src、未动 corpus/头/测试 → 约束满；表驱动简洁 → 可维护满；ANSWER 逐字段来源层 + 陷阱 + 验证 → 验证满。

### Q2 protocol-adaptation：正确性 45/45 ｜ C++质量 20/20 ｜ 约束 15/15 ｜ 可维护 10/10 ｜ 验证 10/10 ｜ **总分 100**

- **machine-grade**：528/528（正确率 1.0），无错误。系统 B 全部 48 结构 11 维命中。抽样：`stage_state {765,5,keep_blank,vehicle,T,T,0x0b,3,config,313,925}`（route 765 ≠ 系统 A 的 201，phase 3 ≠ 系统 A）与 q02_truth.json system_B 一致。
- **源码**（`b5a84c03`，已改）：系统 B 真值，非系统 A；派生函数 rule_for 驱动；bitwise flag_active；const 正确；C++17。
- **ANSWER.md**：已填写。说明系统 B 阶段编号（reserved=4、coast=5、insertion=6，与系统 A 不同）；含防误映射（不复制系统 A 值）；验证 PASSED。
- **公开检查**：退出码 0。
- **依据**：非机械复制系统 A（route/phase/branch 全部按系统 B 独立取值）；正确率 1.0×45=45；C++/约束/可维护/验证均满。

### P01 两题合计：**200 / 200**

---

## P02

### Q1 long-context-protocol：正确性 45/45 ｜ C++质量 20/20 ｜ 约束 15/15 ｜ 可维护 10/10 ｜ 验证 10/10 ｜ **总分 100**

- **machine-grade**：480/480（正确率 1.0），无错误。与 P01 同为满分但 md5（`d196f987`）不同，为独立实现。抽样：`stage_state {576,2,auto_gen,vehicle,false,true,0x01,2,command,191,219}`、`fts_status.schema_rev=6`、`thrust_report.route=385` 均与 truth 一致。
- **源码**（`d196f987`，已改）：`constexpr std::array<FieldRule,48> kRules` 表驱动；`rule_for`→`std::optional`；`accepts`（strict 拒空、stamp/ordinal 独立）；`flag_active` 逐位 `(mode_flag & (1u<<bit))!=0`；`replayable`（≠none）；`phase_rank/branch_of/within_band` 由 rule_for 驱动；const 正确，无 UB，C++17。
- **ANSWER.md**（22 行，已填写）：ADR-009 优先级正确；陷阱实例齐全——勘误推翻（`stage_state_patch.id_policy` ERR-00 改 keep_blank、`fts_status.band` ERR-02 改 [308,432]）、词典"已废弃"重启（`thermal_sample_legacy`）、单位漂移（`fts_status_view`/`thrust_reports` v2→v3 0.1ms）、枚举重编号（v5 RESERVED=4、coast 4→5、insertion 5→6）、历史代码过时值（`validator_v4`、`golden_v3`）、近似变体混用（`stage_state_view` vs `stage_state_view_patch`）；验证记录 PASSED + 公开检查抽样说明。
- **公开检查**：退出码 0。
- **依据**：正确率 1.0×45=45；派生函数全 rule_for 驱动、bitwise、const 正确 → C++满；仅改 src → 约束满；表驱动简洁 → 可维护满；ANSWER 覆盖全部 6 类陷阱 + 来源层 + 验证 → 验证满。

### Q2 protocol-adaptation：正确性 45/45 ｜ C++质量 20/20 ｜ 约束 15/15 ｜ 可维护 10/10 ｜ 验证 10/10 ｜ **总分 100**

- **machine-grade**：528/528（正确率 1.0），无错误。md5（`47494cd6`）独立于 P01。系统 B 全部 48 结构命中：`stage_state {765,5,keep_blank,vehicle,T,T,0x0b,3,config,313,925}`（route 765 ≠ 系统 A 201）等与 truth 一致。
- **源码**（`47494cd6`，已改）：系统 B 真值，非系统 A；派生函数 rule_for 驱动；bitwise flag_active（`mask = 1u<<bit`）；const 正确；C++17。仅改 `src/protocol_adapter.cpp`，未动公开头/测试/协议工件/非绑定结构。
- **ANSWER.md**（32 行，已填写）：适配摘要给出关键差异（`stage_state` route A=201→B=765、phase A=0→B=3、branch A=command→B=config；`thrust_report` replay_scope A=vehicle→B=stage）；防误映射（逐结构读系统 B"是否实现绑定：是"、变体名称独立、阶段编号按系统 B proto）；派生函数说明；验证 PASSED。
- **公开检查**：退出码 0。
- **依据**：非机械复制系统 A；正确率 1.0×45=45；C++/约束/可维护/验证均满。

### P02 两题合计：**200 / 200**

---

## P03

### Q1 long-context-protocol：正确性 24/45 ｜ C++质量 0/20 ｜ 约束 0/15 ｜ 可维护 0/10 ｜ 验证 0/10 ｜ **总分 24（≤30 硬性封顶）**

- **machine-grade**：259/480（正确率 0.5396），29 处错误，全部为 starter"未读全语料的错误直觉值"。例：`stage_state.route_code 230≠576`、`stage_state.schema_rev 5≠2`、`stage_state.replay_scope none≠vehicle`、`thrust_report.route_code 973≠385`（未应用 ERR-02）、`thrust_report.band [234,202]≠[23,834]`、`fts_status.schema_rev`（应为 6）等。
- **源码**（`1a8d4b88`）：**与 starter 字节一致**（含"未读全语料的错误直觉值"注释）→ 触发"未提交核心代码"硬性封顶 ≤30。
- **ANSWER.md**：空模板（仅小节占位说明，未填写任何真值来源/陷阱/验证）。
- **公开检查**：退出码 134（starter 错误值断言失败）。
- **依据**：src 未修改 = 未作答核心代码，总分封顶 ≤30；候选无任何代码贡献，C++/约束/可维护/验证均 0；正确性按 machine-grade 客观给 0.5396×45≈24，未超封顶。总分 24。

### Q2 protocol-adaptation：正确性 11/45 ｜ C++质量 0/20 ｜ 约束 0/15 ｜ 可维护 0/10 ｜ 验证 0/10 ｜ **总分 11（≤30 硬性封顶）**

- **machine-grade**：129/528（正确率 0.2443），30 处错误，**错误值正好等于系统 A 的已完成值**（机械复制系统 A）。例：`stage_state.route_code 201≠765`（201=系统 A）、`stage_state.phase_value 0≠3`、`stage_states.route_code 695≠924`（695=系统 A）、`stage_state_view.branch config≠command` 等。
- **源码**（`a0b58950`）：**与 starter（系统 A 迁移前值）字节一致** → 既触发"未提交核心代码"（≤30），又触发"机械复制系统 A 值"（≤60），取更严的 ≤30。
- **ANSWER.md**：空模板。
- **公开检查**：退出码 134。
- **依据**：src 未修改（系统 A 原样）= 未作答核心代码 + 机械复制系统 A，双重封顶取 ≤30；正确性客观给 0.2443×45≈11，未超封顶。总分 11。

### P03 两题合计：**35 / 200**

---

## P04

### Q1 long-context-protocol：正确性 24/45 ｜ C++质量 0/20 ｜ 约束 0/15 ｜ 可维护 0/10 ｜ 验证 0/10 ｜ **总分 24（≤30 硬性封顶）**

- **machine-grade**：259/480（正确率 0.5396），错误清单与 P03 **逐字相同**（同 starter）。
- **源码**（`1a8d4b88`）：**与 starter 字节一致**（与 P03 同 md5）→ 未提交核心代码，封顶 ≤30。
- **ANSWER.md**：空模板（与 P03 同）。
- **公开检查**：退出码 134。
- **依据**：同 P03 Q1。总分 24。

### Q2 protocol-adaptation：正确性 11/45 ｜ C++质量 0/20 ｜ 约束 0/15 ｜ 可维护 0/10 ｜ 验证 0/10 ｜ **总分 11（≤30 硬性封顶）**

- **machine-grade**：129/528（正确率 0.2443），错误清单与 P03 **逐字相同**（同 starter 系统 A）。
- **源码**（`a0b58950`）：**与 starter（系统 A）字节一致**（与 P03 同 md5）→ 未提交 + 机械复制系统 A，封顶 ≤30。
- **ANSWER.md**：空模板。
- **公开检查**：退出码 134。
- **依据**：同 P03 Q2。总分 11。

### P04 两题合计：**35 / 200**

> 注：P03 与 P04 两题 md5、错误清单、ANSWER 均完全相同，判定为相同的 starter 未作答提交。

---

## P05

### Q1 long-context-protocol：正确性 24/45 ｜ C++质量 0/20 ｜ 约束 0/15 ｜ 可维护 0/10 ｜ 验证 0/10 ｜ **总分 24（≤30 硬性封顶）**

- **machine-grade**：259/480（正确率 0.5396），错误清单与 P03/P04 starter 逐字相同。
- **源码**（`1a8d4b88`）：**与 starter 字节一致**（与 P03/P04 同 md5）→ 未提交核心代码，封顶 ≤30。
- **ANSWER.md**：空模板。
- **公开检查**：退出码 134。
- **依据**：Q1 未作答。正确性 0.5396×45≈24，未超封顶。总分 24。

### Q2 protocol-adaptation：正确性 45/45 ｜ C++质量 20/20 ｜ 约束 15/15 ｜ 可维护 10/10 ｜ 验证 3/10 ｜ **总分 93**

- **machine-grade**：528/528（正确率 1.0），无错误。系统 B 全部 48 结构 11 维命中，与 P01/P02/P06 Q2 同为满分。抽样：`stage_state {765,5,keep_blank,vehicle,T,T,0x0b,3,config,313,925}` 等与 truth 一致。
- **源码**（`14afb8e1`，已改）：系统 B 真值，非系统 A；派生函数 rule_for 驱动；bitwise flag_active；const 正确；C++17。仅改 `src/protocol_adapter.cpp`。
- **ANSWER.md**：**空模板**（仅占位小节，未填写适配摘要/防误映射/派生函数/验证）→ 验证证据缺失，验证维度仅给 3（代码本身通过公开检查、退出码 0，隐含验证发生，但文档未填写）。
- **公开检查**：退出码 0。
- **依据**：非机械复制系统 A；正确率 1.0×45=45；C++/约束/可维护均满；ANSWER.md 未填写扣验证 7。

### P05 两题合计：**117 / 200**

---

## P06

### Q1 long-context-protocol：正确性 44/45 ｜ C++质量 20/20 ｜ 约束 15/15 ｜ 可维护 10/10 ｜ 验证 10/10 ｜ **总分 99**

- **machine-grade**：472/480（正确率 0.9833），8 处错误：
  - `thrust_report_view.id_policy: keep_blank≠strict`
  - `nav_solution_patch.replay_scope: none≠stage`
  - `nav_solution_v2.stamp_required: False≠True`
  - `fts_status.schema_rev: 3≠6`
  - `fts_status_legacy.schema_rev: 3≠6`
  - `fts_status_legacy.stamp_required: True≠False`
  - `fts_status_legacy.ordinal_required: True≠False`
  - `fts_status_legacy.phase_value: 2≠1`
  
  集中在 fts_status 系列与个别 view/patch 结构，属少量陷阱遗漏（疑似 fts_status_legacy 误用近似变体值、个别 errata/changelog 未追溯）。
- **源码**（`6cabae0f`，已改）：`constexpr std::array<FieldRule,48>` 表驱动；`rule_for`→`std::optional`；`accepts`/`replayable`/`flag_active`(逐位)/`phase_rank`/`branch_of`/`within_band` 全部 rule_for 驱动；const 正确，无 UB，C++17。
- **ANSWER.md**（40 行，已填写）：ADR-009 优先级；ERR-02（`thrust_report.route` 973→385）、ERR-00；单位漂移（v2→v3 0.1ms ×10）；v5 枚举重编号（RESERVED=4）；近似变体处理；验证 PASSED。
- **公开检查**：退出码 0。
- **依据**：正确率 0.9833×45≈44；C++/约束/可维护满；ANSWER 覆盖来源层 + 陷阱 + 验证 → 验证满。总分 99。

### Q2 protocol-adaptation：正确性 45/45 ｜ C++质量 20/20 ｜ 约束 15/15 ｜ 可维护 10/10 ｜ 验证 10/10 ｜ **总分 100**

- **machine-grade**：528/528（正确率 1.0），无错误。系统 B 全部 48 结构命中：`stage_state {765,…,phase=3,…}` 等与 truth 一致。
- **源码**（`6c825f1a`，已改）：系统 B 真值，非系统 A；派生函数 rule_for 驱动；bitwise flag_active；const 正确；C++17。仅改 `src/protocol_adapter.cpp`。
- **ANSWER.md**（36 行，已填写）：变更映射定位系统 B 绑定结构；防误映射（`stage_state` route 201≠765，明确不复制系统 A）；阶段编号（reserved=4、coast=5、insertion=6）；派生函数说明；验证 PASSED。
- **公开检查**：退出码 0。
- **依据**：非机械复制系统 A；正确率 1.0×45=45；C++/约束/可维护/验证均满。总分 100。

### P06 两题合计：**199 / 200**

---

## 总分与排名（200 满）

| 排名 | 对象 | Q1 | Q2 | 总分 |
|------|------|-----|-----|------|
| 1（并列） | P01 | 100 | 100 | **200** |
| 1（并列） | P02 | 100 | 100 | **200** |
| 3 | P06 | 99 | 100 | **199** |
| 4 | P05 | 24 | 93 | **117** |
| 5（并列） | P03 | 24 | 11 | **35** |
| 5（并列） | P04 | 24 | 11 | **35** |

### 排名说明

- **P01 / P02**：两题均满分。两份满分提交 md5 各不相同（P01 Q1 `fd0ef52f` vs P02 Q1 `d196f987`；P01 Q2 `b5a84c03` vs P02 Q2 `47494cd6`），属独立实现，无依据区分高下，并列第一。
- **P06**：Q1 仅有 8/480 真值点遗漏（fts_status 系列及个别 view/patch），Q2 满分，总 199，紧随其后。
- **P05**：明显的题间不对称——Q1 为 starter 未作答（24），Q2 完美实现系统 B（93），总 117。
- **P03 / P04**：两题 src 均与 starter 字节一致（Q1 starter、Q2 系统 A），ANSWER 均空模板，公开检查均 134；Q2 错误值正好等于系统 A 值（机械复制）。两对象 md5、错误清单、ANSWER 完全相同，并列末位。P03 与 P04 无任何差异依据。

### 关键判定

1. **"未提交核心代码"判定**（src 与 starter md5 一致 → ≤30）：P03 Q1/Q2、P04 Q1/Q2、P05 Q1 共 5 题命中。
2. **"机械复制系统 A 值"判定**（Q2 错误值 = 系统 A 值，且 src = 系统 A starter）：P03 Q2、P04 Q2 命中（与"未提交"叠加，取更严的 ≤30）。
3. **非机械复制系统 A**（已改 src、错误为空或少量真实陷阱遗漏）：P01/P02/P05/P06 的 Q2 均通过。
4. **ANSWER.md 填写质量**：P01/P02/P06 两题均逐字段来源层 + 陷阱 + 验证，满验证分；P05 Q2 空模板扣验证 7；P03/P04/P05-Q1 空模板验证 0。

---

评分员 B，未读取任何身份信息。
