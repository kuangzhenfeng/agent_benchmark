# Q2 作答说明：系统 A → 系统 B 协议适配

## 解题结论

仅修改 `src/protocol_adapter.cpp` 中的 `kRules` 数组，把起始的【系统 A 迁移前值】整体替换为【系统 B 的最终真值】（48 个实现绑定结构 × 10 个适配维度）。未修改公开头文件、测试、系统 A、系统 B、`变更映射.json` 中任何文件。派生函数（`rule_for/accepts/replayable/flag_active/phase_rank/branch_of/within_band`）一字未动。

构建与验证：`g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread`，`./run_public_checks.sh` 退出码 0（全部 8 个结构锚点逐字段 + `flag_active` 抽样断言通过，零编译警告）。

---

## 适配方法（真值来源）

**所有 480 个字段（48 结构 × 10 维）一律逐字段读取自系统 B 物模型注释中各绑定结构的『适配契约元数据』块**——即 `系统B/{action,config,event,status}.proto` 里每个『是否实现绑定: 是』结构紧跟的契约注释块（路由码 / 小版本 / 标识策略 / 回放域 / 时间戳 / 序号 / 模式位 / 飞行阶段 / 载荷分支 / 合法区间）。

关键原则（严格遵守题目约束）：
1. **不复制系统 A**：`系统A/变更映射.json` 的『协议一变更后(系统A值)』只是迁移动机说明，`迁移提醒` 明确系统 A 卡片不得直接复制——本答案完全不使用系统 A 的任何字段值。起始 cpp 里的系统 A 值被全部覆盖。
2. **不据名称/相邻结构/子系统族推断**：每个绑定结构的 10 个字段都从它自己的契约块独立读取；base/plural/view/patch/legacy/v2 近似变体即使字段不同也绝不互相借用。48 结构散布在 348 个结构之中，非绑定结构（『是否实现绑定: 否』）一律不新增为规则项。
3. **不做 trim/哈希/去重/单位换算**：标识、回放令牌、样本值原样映射，仅做中文→C++ 枚举的字面转换。
4. **飞行阶段一律采用系统 B 枚举编号**（详见下节）。

---

## 阶段编号陷阱（硬上限规避）

系统 A 与系统 B 的 `Phase` 枚举编号不同，**绝不可用系统 A 阶段编号填系统 B**。系统 B 的 `definitions.proto` 枚举为：

| 阶段 | 系统 A 编号 | 系统 B 编号 |
|------|------------|------------|
| idle | 0 | 0 |
| ready(A)/standby(B) | 1 | 1 |
| liftoff(A)/boost(B) | 2 | 2 |
| ascent | 3 | 3 |
| (A 无) / reserved | — | **4**（占位） |
| coast | **4** | **5** |
| insertion | **5** | **6** |

差异点：系统 B 在 4 号位插入了 `reserved` 占位，导致 `coast` 由 A 的 4 变成 B 的 5、`insertion` 由 A 的 5 变成 B 的 6。本答案中每个结构的 `phase_value` 都取系统 B 契约块里『飞行阶段 phase = <名>=<数>』给出的数值，并在提取后用系统 B 枚举交叉核对（提取脚本报告『阶段编号与系统 B 枚举不一致』为空）。

例如：`payload_frame` 的契约块标注 `飞行阶段 phase = coast=5`，故填 `5`（系统 B）；若误用系统 A 的 coast 则会错填成 4，触发硬上限。同理 `sep_event`(insertion=6)、`thrust_report_legacy`(insertion=6) 等使用系统 B 的 6 号位。

---

## 派生函数说明（均由规则表驱动，未改动）

- `rule_for(id)`：线性查 `kRules`，返回该结构的 `FieldRule`（或 `std::nullopt`）。其余函数都先取规则再判断。
- `accepts(id, in)`：仅当 `id_policy == strict` 时拒绝空标识（`in.identifier.empty()`）；`stamp_required` / `ordinal_required` 两个布尔各自独立——前者为真时要求 `in.has_stamp`，后者为真时要求 `in.has_ordinal`，互不影响。
- `replayable(id)`：当且仅当 `replay_scope != none` 时为真（stage/vehicle/range 三种都算可回放）。
- `flag_active(id, bit)`：逐位判断 `mode_flag & (1u << bit)`，**不整体复制模式位含义**；每个 `ModeBit`(arm=0..debug=5) 单独取掩码。
- `phase_rank(id)`：直接返回 `phase_value`（系统 B 编号，见上"阶段编号陷阱"）。
- `branch_of(id)`：返回该结构 `branch`；`within_band(id, sample)`：包含性判断 `sample >= band_min && sample <= band_max`。

---

## 系统 B 真值表（48 结构 × 10 字段）

> 字段顺序：route_code / schema_rev / id_policy / replay_scope / stamp_req / ordinal_req / mode_flag(hex) / phase(系统B编号) / branch / band(min,max)。
> 所有值取自系统 B 契约元数据块；phase 列括注系统 B 阶段名。

### stage 族
- **stage_state**：765 / 5 / keep_blank / vehicle / T / T / 0x0b / 3(ascent=3) / config / (313,925)
- **stage_states**：924 / 7 / keep_blank / none / T / T / 0x1d / 0(idle=0) / event / (295,1082)
- **stage_state_view**：960 / 7 / keep_blank / range / F / T / 0x3e / 2(boost=2) / command / (348,777)
- **stage_state_patch**：694 / 5 / auto_gen / vehicle / F / T / 0x02 / 0(idle=0) / event / (54,739)
- **stage_state_legacy**：105 / 7 / keep_blank / vehicle / T / F / 0x21 / 1(standby=1) / event / (389,498)
- **stage_state_v2**：421 / 5 / keep_blank / vehicle / T / F / 0x17 / 3(ascent=3) / telemetry / (0,112)

### propulsion 族
- **thrust_report**：709 / 8 / keep_blank / stage / T / T / 0x01 / 1(standby=1) / event / (362,486)
- **thrust_reports**：521 / 2 / strict / none / T / F / 0x30 / 0(idle=0) / event / (202,452)
- **thrust_report_view**：389 / 8 / strict / vehicle / T / F / 0x21 / 5(coast=5) / event / (30,435)
- **thrust_report_patch**：216 / 7 / keep_blank / vehicle / F / F / 0x10 / 0(idle=0) / config / (333,1120)
- **thrust_report_legacy**：242 / 9 / auto_gen / vehicle / T / F / 0x0e / 6(insertion=6) / config / (196,864)
- **thrust_report_v2**：453 / 2 / keep_blank / vehicle / T / F / 0x33 / 5(coast=5) / config / (89,811)

### guidance 族
- **nav_solution**：700 / 2 / auto_gen / none / T / F / 0x10 / 1(standby=1) / command / (115,1083)
- **nav_solutions**：583 / 5 / keep_blank / none / T / T / 0x0a / 0(idle=0) / command / (158,406)
- **nav_solution_view**：147 / 8 / keep_blank / none / F / F / 0x06 / 1(standby=1) / command / (67,738)
- **nav_solution_patch**：838 / 2 / keep_blank / none / F / T / 0x03 / 5(coast=5) / event / (419,805)
- **nav_solution_legacy**：617 / 2 / keep_blank / none / T / T / 0x36 / 1(standby=1) / command / (241,796)
- **nav_solution_v2**：701 / 8 / keep_blank / none / T / T / 0x0b / 0(idle=0) / command / (237,785)

### separation 族
- **sep_event**：786 / 5 / auto_gen / range / F / T / 0x1a / 6(insertion=6) / command / (321,610)
- **sep_events**：625 / 8 / auto_gen / range / T / F / 0x3b / 1(standby=1) / command / (194,1044)
- **sep_event_view**：867 / 6 / auto_gen / none / T / F / 0x3d / 2(boost=2) / command / (253,1129)
- **sep_event_patch**：601 / 5 / keep_blank / none / T / T / 0x1e / 2(boost=2) / config / (31,311)
- **sep_event_legacy**：427 / 4 / keep_blank / vehicle / F / F / 0x23 / 3(ascent=3) / telemetry / (381,1049)
- **sep_event_v2**：644 / 6 / strict / stage / T / F / 0x2b / 1(standby=1) / event / (482,1430)

### rangesafety 族
- **fts_status**：103 / 7 / strict / none / F / T / 0x1a / 0(idle=0) / config / (25,714)
- **fts_statuses**：289 / 3 / auto_gen / none / T / F / 0x35 / 5(coast=5) / event / (17,885)
- **fts_status_view**：915 / 4 / auto_gen / stage / F / T / 0x22 / 2(boost=2) / telemetry / (223,497)
- **fts_status_patch**：809 / 9 / keep_blank / none / F / T / 0x07 / 5(coast=5) / command / (389,955)
- **fts_status_legacy**：476 / 5 / auto_gen / vehicle / T / T / 0x0f / 2(boost=2) / config / (103,1073)
- **fts_status_v2**：429 / 5 / auto_gen / vehicle / T / T / 0x22 / 1(standby=1) / event / (19,159)

### payload 族
- **payload_frame**：108 / 5 / auto_gen / vehicle / F / T / 0x2d / 5(coast=5) / telemetry / (363,994)
- **payload_frames**：593 / 7 / auto_gen / range / T / T / 0x04 / 1(standby=1) / config / (419,832)
- **payload_frame_view**：175 / 9 / strict / vehicle / T / T / 0x03 / 0(idle=0) / telemetry / (99,880)
- **payload_frame_patch**：697 / 5 / keep_blank / none / T / T / 0x21 / 3(ascent=3) / command / (153,173)
- **payload_frame_legacy**：792 / 5 / keep_blank / stage / T / F / 0x03 / 1(standby=1) / telemetry / (202,630)
- **payload_frame_v2**：255 / 1 / strict / range / F / T / 0x1f / 0(idle=0) / event / (36,463)

### groundlink 族
- **link_quality**：239 / 7 / strict / range / T / T / 0x28 / 1(standby=1) / config / (125,249)
- **link_qualities**：683 / 6 / keep_blank / vehicle / F / F / 0x01 / 2(boost=2) / config / (471,791)
- **link_quality_view**：509 / 9 / strict / range / F / T / 0x03 / 0(idle=0) / command / (136,428)
- **link_quality_patch**：386 / 8 / auto_gen / none / T / T / 0x2e / 2(boost=2) / config / (220,302)
- **link_quality_legacy**：424 / 8 / keep_blank / vehicle / T / T / 0x3d / 5(coast=5) / telemetry / (277,1022)
- **link_quality_v2**：923 / 1 / strict / stage / T / T / 0x3b / 0(idle=0) / config / (303,1110)

### thermal 族
- **thermal_sample**：454 / 2 / strict / range / F / T / 0x17 / 1(standby=1) / command / (302,608)
- **thermal_samples**：127 / 5 / keep_blank / vehicle / T / F / 0x3c / 5(coast=5) / telemetry / (214,613)
- **thermal_sample_view**：716 / 3 / auto_gen / range / T / F / 0x3b / 0(idle=0) / config / (330,629)
- **thermal_sample_patch**：821 / 7 / auto_gen / none / T / T / 0x02 / 3(ascent=3) / telemetry / (447,1033)
- **thermal_sample_legacy**：749 / 4 / strict / none / F / F / 0x1a / 3(ascent=3) / event / (494,908)
- **thermal_sample_v2**：316 / 3 / strict / stage / F / T / 0x02 / 2(boost=2) / event / (121,234)

---

## 验证

1. **公开检查（`./run_public_checks.sh`）**：退出码 0，通过。
   - 8 个结构锚点逐字段（route/rev/id_policy/replay_scope/stamp/ordinal/flag/phase/branch/band_min/band_max）全部匹配：stage_state、thrust_reports、thrust_report_v2、nav_solution_legacy、sep_event_v2、payload_frame、link_qualities、thermal_sample_patch。
   - `flag_active` 对 stage_state（mode_flag=0x0b）断言：arm=true、ignite=true、hold=false、safe=true、redundant=false、debug=false，逐位全部通过（0x0b = bit0+bit1+bit3，对应 arm/ignite/safe 置位）。
   - `within_band` 边界（含 band_min、band_max）随锚点一并校验。

2. **真值交叉核对（离线脚本）**：用脚本解析系统 B 四份 proto，定位每个『是否实现绑定: 是』结构，向下收集到『合法区间 band』的契约块，正则提取 10 个字段，组装 48×10 真值表。脚本自检结果：
   - 提取绑定结构数 = 48（与 8 子系统族 × 6 变体相符）；
   - 缺失结构 = 无；字段不完整 = 无；
   - 阶段编号与系统 B 枚举不一致 = 无（确认全部使用系统 B 编号，未误用系统 A 编号）；
   - 与 `tests/public_checks.cpp` 的 8 个基准真值逐字段比对，全部匹配。
   脚本仅用于离线核对，未引入提交产物（提交保持纯 C++17、无外部依赖、无 protobuf）。

3. **编译警告**：`-Wall -Wextra -Wpedantic` 下零警告通过。

### 仍未覆盖的风险

- 公开检查只覆盖 8/48 结构的逐字段断言；其余 40 个结构的真值依赖上述离线脚本对系统 B 契约块的逐字段提取，未逐一写进测试（题目限定不得修改测试）。这些字段同样直接取自系统 B 契约块，与锚点字段采用同一套提取规则。
- `mode_flag` 仅对 stage_state 做了位抽样断言；其余结构的位语义靠 `mode_flag` 数值本身的正确性保证（数值来自系统 B 契约块）。
- 阶段编号差异是最主要的硬上限风险点，已通过系统 B 枚举交叉核对规避；其余字段（路由码、区间、策略等）系统 A 与系统 B 无编号错位问题，但仍逐字段从系统 B 独立读取，未与系统 A 对齐。
