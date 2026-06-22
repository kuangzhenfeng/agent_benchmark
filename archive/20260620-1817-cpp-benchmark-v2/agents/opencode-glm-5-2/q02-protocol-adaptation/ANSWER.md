# Q2 作答说明

## 适配摘要

仅适配系统 B(orbit.tc)中 "是否实现绑定: 是" 的 48 个结构。真值来源是 `系统B/*.proto` 中每个绑定结构注释块里的 "适配契约元数据"，逐字段独立读取，未复制系统 A 值。

48 个绑定结构在 4 个 proto 文件中各分布 12 个（按子系统族划分）：
- `系统B/status.proto`：stage 族 6 个 + rangesafety 族 6 个
- `系统B/action.proto`：propulsion 族 6 个 + payload 族 6 个
- `系统B/config.proto`：guidance 族 6 个 + groundlink 族 6 个
- `系统B/event.proto`：separation 族 6 个 + thermal 族 6 个

每个结构 10 个字段维度的最终真值见 `src/protocol_adapter.cpp` 的 `kRules`。下表摘录每结构的路由码/小版本/标识/回放/时间戳/序号/模式位/阶段/分支/区间：

| 结构 | route | rev | id_policy | replay | stamp | ordinal | mode | phase | branch | band |
|------|------:|----:|-----------|--------|:-----:|:-------:|-----:|------:|--------|------|
| stage_state | 765 | 5 | keep_blank | vehicle | T | T | 11 | 3 | config | [313,925] |
| stage_states | 924 | 7 | keep_blank | none | T | T | 29 | 0 | event | [295,1082] |
| stage_state_view | 960 | 7 | keep_blank | range | F | T | 62 | 2 | command | [348,777] |
| stage_state_patch | 694 | 5 | auto_gen | vehicle | F | T | 2 | 0 | event | [54,739] |
| stage_state_legacy | 105 | 7 | keep_blank | vehicle | T | F | 33 | 1 | event | [389,498] |
| stage_state_v2 | 421 | 5 | keep_blank | vehicle | T | F | 23 | 3 | telemetry | [0,112] |
| thrust_report | 709 | 8 | keep_blank | stage | T | T | 1 | 1 | event | [362,486] |
| thrust_reports | 521 | 2 | strict | none | T | F | 48 | 0 | event | [202,452] |
| thrust_report_view | 389 | 8 | strict | vehicle | T | F | 33 | 5 | event | [30,435] |
| thrust_report_patch | 216 | 7 | keep_blank | vehicle | F | F | 16 | 0 | config | [333,1120] |
| thrust_report_legacy | 242 | 9 | auto_gen | vehicle | T | F | 14 | 6 | config | [196,864] |
| thrust_report_v2 | 453 | 2 | keep_blank | vehicle | T | F | 51 | 5 | config | [89,811] |
| nav_solution | 700 | 2 | auto_gen | none | T | F | 16 | 1 | command | [115,1083] |
| nav_solutions | 583 | 5 | keep_blank | none | T | T | 10 | 0 | command | [158,406] |
| nav_solution_view | 147 | 8 | keep_blank | none | F | F | 6 | 1 | command | [67,738] |
| nav_solution_patch | 838 | 2 | keep_blank | none | F | T | 3 | 5 | event | [419,805] |
| nav_solution_legacy | 617 | 2 | keep_blank | none | T | T | 54 | 1 | command | [241,796] |
| nav_solution_v2 | 701 | 8 | keep_blank | none | T | T | 11 | 0 | command | [237,785] |
| sep_event | 786 | 5 | auto_gen | range | F | T | 26 | 6 | command | [321,610] |
| sep_events | 625 | 8 | auto_gen | range | T | F | 59 | 1 | command | [194,1044] |
| sep_event_view | 867 | 6 | auto_gen | none | T | F | 61 | 2 | command | [253,1129] |
| sep_event_patch | 601 | 5 | keep_blank | none | T | T | 30 | 2 | config | [31,311] |
| sep_event_legacy | 427 | 4 | keep_blank | vehicle | F | F | 35 | 3 | telemetry | [381,1049] |
| sep_event_v2 | 644 | 6 | strict | stage | T | F | 43 | 1 | event | [482,1430] |
| fts_status | 103 | 7 | strict | none | F | T | 26 | 0 | config | [25,714] |
| fts_statuses | 289 | 3 | auto_gen | none | T | F | 53 | 5 | event | [17,885] |
| fts_status_view | 915 | 4 | auto_gen | stage | F | T | 34 | 2 | telemetry | [223,497] |
| fts_status_patch | 809 | 9 | keep_blank | none | F | T | 7 | 5 | command | [389,955] |
| fts_status_legacy | 476 | 5 | auto_gen | vehicle | T | T | 15 | 2 | config | [103,1073] |
| fts_status_v2 | 429 | 5 | auto_gen | vehicle | T | T | 34 | 1 | event | [19,159] |
| payload_frame | 108 | 5 | auto_gen | vehicle | F | T | 45 | 5 | telemetry | [363,994] |
| payload_frames | 593 | 7 | auto_gen | range | T | T | 4 | 1 | config | [419,832] |
| payload_frame_view | 175 | 9 | strict | vehicle | T | T | 3 | 0 | telemetry | [99,880] |
| payload_frame_patch | 697 | 5 | keep_blank | none | T | T | 33 | 3 | command | [153,173] |
| payload_frame_legacy | 792 | 5 | keep_blank | stage | T | F | 3 | 1 | telemetry | [202,630] |
| payload_frame_v2 | 255 | 1 | strict | range | F | T | 31 | 0 | event | [36,463] |
| link_quality | 239 | 7 | strict | range | T | T | 40 | 1 | config | [125,249] |
| link_qualities | 683 | 6 | keep_blank | vehicle | F | F | 1 | 2 | config | [471,791] |
| link_quality_view | 509 | 9 | strict | range | F | T | 3 | 0 | command | [136,428] |
| link_quality_patch | 386 | 8 | auto_gen | none | T | T | 46 | 2 | config | [220,302] |
| link_quality_legacy | 424 | 8 | keep_blank | vehicle | T | T | 61 | 5 | telemetry | [277,1022] |
| link_quality_v2 | 923 | 1 | strict | stage | T | T | 59 | 0 | config | [303,1110] |
| thermal_sample | 454 | 2 | strict | range | F | T | 23 | 1 | command | [302,608] |
| thermal_samples | 127 | 5 | keep_blank | vehicle | T | F | 60 | 5 | telemetry | [214,613] |
| thermal_sample_view | 716 | 3 | auto_gen | range | T | F | 59 | 0 | config | [330,629] |
| thermal_sample_patch | 821 | 7 | auto_gen | none | T | T | 2 | 3 | telemetry | [447,1033] |
| thermal_sample_legacy | 749 | 4 | strict | none | F | F | 26 | 3 | event | [494,908] |
| thermal_sample_v2 | 316 | 3 | strict | stage | F | T | 2 | 2 | event | [121,234] |

## 防止误映射的做法

1. **用变更映射定位**：依据 `系统A/变更映射.json` 的 "协议二结构" 字段，确认每个 C++ 结构代号对应的 orbit.tc 结构名；再在 `系统B/*.proto` 中找到 "是否实现绑定: 是" 且 "结构代号" 与之一致的注释块，读取其 "适配契约元数据"。
2. **不复制系统 A 值**：系统 A 的值（含 `变更映射.json` 的 "协议一变更后(系统A值)" 与 `系统A/*.proto` 注释）仅用于理解迁移动机，绝不写入规则表。例：stage_state 在系统 A 是 route=201/phase=idle(0)/branch=command，系统 B 实际为 route=765/phase=ascent(3)/branch=config，两者完全不同。
3. **近似变体不混用**：系统 B 有约 300 个非绑定结构带近似名（_records/_log/_summary/_v3/_legacy2/_patches/_views/_mirror/_shadow/_ctx/_snapshot/_raw/_delta/_history/_0/_1/_2 等）。仅取 "是否实现绑定: 是" 且名称与 `enum class Structure` 完全一致的 48 个；非绑定结构一律不新增规则项。已核验 4 个 proto 文件中 "实现绑定: 是" 的结构恰好 48 个（每文件 12 个）。
4. **阶段编号按系统 B**：系统 A 的 Phase 枚举为 IDLE=0/READY=1/LIFTOFF=2/ASCENT=3/COAST=4/INSERTION=5；系统 B 为 IDLE=0/STANDBY=1/BOOST=2/ASCENT=3/RESERVED=4/COAST=5/INSERTION=6。系统 B 注释以 "phase = 名称=编号" 同时给出名称与编号，`phase_value` 取系统 B 编号。关键差异：coast 在 A=4、B=5；insertion 在 A=5、B=6；系统 B 的 4 号位是 reserved 占位（48 个绑定结构均未用到）。例如 thrust_report_v2 阶段=coast=5（非 A 的 4），sep_event 阶段=insertion=6（非 A 的 5）。
5. **不对值做加工**：标识、回放令牌、样本值均原样采用，未做 trim、哈希、去重或单位换算。

## 派生函数说明

均由 `rule_for` 返回的 `FieldRule` 驱动，未按名称/位置/族系生成默认行为：
- `accepts`：仅当 `id_policy == strict` 时拒绝空标识；`stamp_required` 与 `ordinal_required` 互相独立，缺一即拒绝。
- `replayable`：`replay_scope != none` 即为真。
- `flag_active`：`mode_flag` 按位与对应 ModeBit 掩码判断（逐位，未整体复制）。
- `phase_rank`：返回 `phase_value`（系统 B 编号）。
- `branch_of`：返回 `branch`。
- `within_band`：`band_min <= sample <= band_max`。

## 验证

1. **公开检查**：`./run_public_checks.sh` 通过（抽样 8 个结构 + flag_active 逐位）。输出 "Q2 PUBLIC CHECKS PASSED"。
2. **源文件抽样核对**：直接读取了 15 个绑定结构（含 8 个公开抽样 + thrust_report / thrust_report_view / sep_event / thermal_sample_v2 / link_quality_v2 / nav_solution / fts_status）在 `系统B/*.proto` 中的注释块，逐字段与规则表一致。
3. **绑定结构计数核对**：4 个 proto 文件每文件恰好 12 个 "实现绑定: 是" 结构，合计 48，与目标一致；未给任何非绑定结构新增规则项。
4. **阶段编号核对**：确认所有 phase_value 取自系统 B 枚举（coast=5、insertion=6 等），未误用系统 A 编号。
5. **编译**：`g++ -std=c++17 -Wall -Wextra -Wpedantic` 无警告通过。

仍未覆盖的风险：公开检查只抽样 8 项；其余 40 项依靠源文件注释直接读取保证。已通过 15 项人工抽样 + 绑定计数核对 + 阶段枚举交叉验证将风险降到最低。
