# Q1 作答说明

## 文档优先级

ADR-009 规定的取舍优先级（高 → 低）：

**勘误表(errata) > changelog > v6.proto 注释 > 字段词典(v4) > 历史代码**

- 勘误表为变更后的最终事实，任何与之冲突的 proto 注释/changelog/词典一律以勘误表为准。
- changelog 次之；proto 注释里写 "详见 changelog/..." 的字段，按对应 changelog 给出的最终值填写。
- 字段词典停留在 v4、历史代码/历史测试含已知缺陷，仅作最低优先级参考，本次未让它们决定任何字段的最终值。

冲突解决流程：对每个字段，先取 v6.proto 注释的字面值；若该字段是 "详见 changelog" 指针，到对应 changelog 取值；再用 12 份勘误表逐条覆盖；最终值即为规则表中的值。

## v6 真值来源（逐结构取舍要点）

下表只列被勘误表/changelog 推翻的字段（其余字段取 v6.proto 注释字面值）。完整 48×10 真值见 `src/range_validator.cpp` 的 `kRules`。

| 结构 | 被推翻字段（低优先值 → 最终值 / 来源） |
|------|----------------------------------------|
| stage_state | route 230→576(ERR-07); schema 5→2(ERR-09); replay 不回放→本箭(ERR-11); ordinal F→T(ERR-09); id_policy/branch=changelog v5→v6; phase=2(v4→v5) |
| stage_states | id_policy 保留空值→严格必填(ERR-08); band=changelog v5→v6 [85,201] |
| stage_state_view | branch 事件→指令(ERR-05); route/id_policy=changelog v4→v5; phase 1→5; band=changelog v2→v3(单位漂移,值不变)[353,784] |
| stage_state_patch | id_policy 严格必填→保留空值(ERR-00); ordinal F→T(ERR-09); schema=changelog v4→v5=5; mode 36→42 |
| stage_state_legacy | replay 本级→本靶区(ERR-01); ordinal F→T(ERR-09); route/schema/id_policy=changelog; phase=2 |
| stage_state_v2 | schema 1→7(ERR-09); replay 本级→本靶区(ERR-09); stamp T→F(ERR-11); id_policy/branch=changelog; band=changelog v2→v3 [75,622] |
| thrust_report | route 973→385(ERR-02); stamp T→F(ERR-07); ordinal T→F(ERR-04); schema=changelog v5→v6=2; phase 6→5 |
| thrust_reports | schema 8→2(ERR-07); id_policy 空则生成→保留空值(ERR-09); replay 本靶区→本级(ERR-09); branch=changelog v5→v6=遥测; band=changelog v2→v3 [449,916] |
| thrust_report_view | schema/id_policy/branch=changelog v4→v5; replay=changelog v5→v6=不回放; band [134,409]→[71,559](ERR-08) |
| thrust_report_patch | stamp F→T(ERR-07); ordinal F→T(ERR-06); phase 1→3(v4→v5) |
| thrust_report_legacy | stamp T→F(ERR-03); phase 1→6(v4→v5) |
| thrust_report_v2 | route 821→986(ERR-03); schema 6→7(ERR-11); replay 本级→本箭(ERR-03); ordinal F→T(ERR-01); branch=changelog v4→v5=遥测 |
| nav_solution | schema 9→4(ERR-01); replay 本级→不回放(ERR-03); ordinal F→T(ERR-05); mode 55→59(v4→v5); phase/band=changelog v5→v6 (6 / [290,612]) |
| nav_solutions | schema 2→8(ERR-01); id_policy 保留空值→严格必填(ERR-01); ordinal T→F(ERR-08); replay/branch=changelog v4→v5; mode 8→14; phase 3→6; band=changelog v5→v6 [408,794] |
| nav_solution_view | replay 本箭→本级(ERR-08); stamp T→F(ERR-07); ordinal F→T(ERR-09); band [181,84]→[220,728](ERR-03); id_policy/branch=changelog; mode 8→54; phase=1 |
| nav_solution_patch | stamp T→F(ERR-06); ordinal T→F(ERR-01); route/schema/branch/band=changelog (798/3/指令/[98,1018]); replay=changelog v4→v5=本级 |
| nav_solution_legacy | ordinal T→F(ERR-05); route/schema/id_policy/replay/branch=changelog; phase 0→6(v4→v5) |
| nav_solution_v2 | schema 8→2(ERR-09); id_policy 空则生成→保留空值(ERR-10); ordinal T→F(ERR-11); band [186,123]→[351,716](ERR-04); mode 16→4; branch=changelog=事件 |
| sep_event | branch 指令→配置(ERR-10); band [98,345]→[464,557](ERR-04); route/id_policy/replay=changelog; mode 55→9 |
| sep_events | schema 1→8(ERR-02); id_policy 空则生成→保留空值(ERR-01); replay 本箭→本靶区(ERR-09); ordinal T→F(ERR-04); branch 配置→事件(ERR-05); mode 4→9; phase 0→2 |
| sep_event_view | schema 2→9(ERR-11); stamp T→F(ERR-11); band [414,339]→[341,1082](ERR-04); route/id_policy/replay=changelog; phase 1→0 |
| sep_event_patch | ordinal T→F(ERR-10); branch 指令→配置(ERR-04); schema/replay=changelog |
| sep_event_legacy | schema 1→4(ERR-11); replay 本级→本靶区(ERR-09); ordinal T→F(ERR-10); id_policy/band=changelog v5→v6 |
| sep_event_v2 | schema 1→6(ERR-11); replay 本靶区→本箭(ERR-01); branch 配置→遥测(ERR-03) |
| fts_status | stamp T→F(ERR-08); band [110,285]→[308,432](ERR-02); schema/replay/branch=changelog; mode 54→4 |
| fts_statuses | schema 2→3(ERR-06); replay 本级→本箭(ERR-09); ordinal T→F(ERR-11); id_policy/phase=changelog v4→v5 |
| fts_status_view | stamp T→F(ERR-07); branch 指令→遥测(ERR-03); band=changelog v2→v3 [445,721]; id_policy=changelog |
| fts_status_patch | id_policy 严格必填→空则生成(ERR-05); replay 本级→不回放(ERR-07); stamp T→F(ERR-10); phase 5→0; branch=changelog=事件; band=changelog v2→v3 [2,390] |
| fts_status_legacy | schema/branch=changelog v5→v6/v4→v5; band=changelog v2→v3 [209,233] |
| fts_status_v2 | replay 本靶区→本级(ERR-09); route/branch=changelog |
| payload_frame | mode 55→51(v4→v5)。其余取 proto 字面值，无推翻。 |
| payload_frames | replay 不回放→本靶区(ERR-09); route/schema=changelog; mode 32→31; band=changelog v2→v3 [277,1162] |
| payload_frame_view | route 175→972(ERR-04); id_policy 严格必填→保留空值(ERR-02); ordinal F→T(ERR-02); schema/replay=changelog |
| payload_frame_patch | route 459→667(ERR-06); replay 本箭→本靶区(ERR-01); stamp T→F(ERR-10); branch 遥测→指令(ERR-02); band [316,463]→[484,812](ERR-11); schema=changelog; mode 35→17 |
| payload_frame_legacy | schema 8→1(ERR-11); id_policy 空则生成→严格必填(ERR-07); stamp F→T(ERR-02); branch 指令→事件(ERR-10); route=changelog v5→v6 |
| payload_frame_v2 | schema 4→6(ERR-08); replay 本箭→本级(ERR-01); ordinal F→T(ERR-01); band [300,295]→[413,705](ERR-09); route/branch=changelog |
| link_quality | route 664→412(ERR-06); ordinal T→F(ERR-00); branch 配置→遥测(ERR-03); id_policy/replay=changelog |
| link_qualities | id_policy 严格必填→保留空值(ERR-10); branch 配置→指令(ERR-06); schema/band=changelog v4→v5 |
| link_quality_view | id_policy 严格必填→空则生成(ERR-03); replay 本靶区→不回放(ERR-10); schema=changelog v5→v6; mode 8→12; branch=changelog=指令 |
| link_quality_patch | id_policy 严格必填→保留空值(ERR-09); stamp T→F(ERR-08); ordinal T→F(ERR-09); route/schema/phase=changelog |
| link_quality_legacy | ordinal F→T(ERR-10); schema/id_policy/replay/phase/branch=changelog |
| link_quality_v2 | schema 7→4(ERR-10); id_policy 保留空值→严格必填(ERR-03); stamp F→T(ERR-10); band [27,160]→[374,931](ERR-04); replay=changelog |
| thermal_sample | schema 6→7(ERR-10); ordinal F→T(ERR-04); branch 配置→事件(ERR-09); id_policy=changelog; mode 61→18; phase 2→5 |
| thermal_samples | route 803→978(ERR-09); replay 本箭→本级(ERR-06); stamp F→T(ERR-06); branch=changelog; mode 55→16 |
| thermal_sample_view | schema 1→9(ERR-08); id_policy 严格必填→保留空值(ERR-08); replay 本靶区→不回放(ERR-02); stamp T→F(ERR-09); ordinal F→T(ERR-02); branch=changelog v5→v6=事件 |
| thermal_sample_patch | schema/id_policy/band=changelog v5→v6; stamp F→T(ERR-07); ordinal F→T(ERR-02); phase/branch=changelog v4→v5 |
| thermal_sample_legacy | schema 5→9(ERR-09); route/branch=changelog |
| thermal_sample_v2 | branch 遥测→配置(ERR-05); route/schema=changelog; phase 2→5 |

## 陷阱处理（举例）

- **被勘误推翻的 proto 注释**：`link_quality.ordinal_required` proto 记 true，ERR-00 更正为 false；`thrust_report.route_code` proto 记 973，ERR-02 更正为 385。一律以勘误为准。
- **字段词典"已废弃"实为重新启用**：本次最终值未依赖 v4 词典；词典仅作最低优先级参考，未让"已废弃"标注影响取舍。
- **单位漂移（ms → 0.1ms）**：proto 注释里 "单位自 v3 起变更, 见 changelog/v2-to-v3.md" 的 band 字段（stage_state_view、stage_state_v2、thrust_reports、fts_status_view、fts_status_patch、fts_status_legacy、payload_frames），按 v2→v3 changelog 该批 band 的数值本身不变，仅单位漂移，故直接采用 proto 字面区间。
- **枚举重编号（飞行阶段）**：v5 起 Phase 枚举新增 RESERVED=4 占位，COAST=5、INSERTION=6。proto 中 "v5 枚举重编号(旧值 X), 以重编号后为准" 的字段，按 proto 第二行给出的重编号后最终值/changelog 取值（如 nav_solution_legacy phase 旧 0→6；thrust_report phase 旧 6→5）。
- **历史代码/历史测试过时值**：validator_v2/v4.cpp 与 golden_v3.cpp 均未作为任何字段的真值来源（ADR-009 中历史代码优先级最低且含已知缺陷）。
- **近似变体名称混用**：corpus 中存在大量近似名（_records/_log/_tmp/_bak/_enriched/_mirror/_shadow/_v3/_legacy2/_patches/_views 等）。仅取与 `enum class Structure` 完全一致的 48 个绑定结构（带 "适配契约(v6):" 注释块）的值；其余 300 个非绑定结构忽略。已用脚本核验 v6.proto 中 48 个 "适配契约" 块全部落在 48 个目标结构上，无误绑。

## 验证

1. **公开检查**：`./run_public_checks.sh` 通过（抽样 8 个结构 + flag_active 逐位）。输出 "Q1 PUBLIC CHECKS PASSED"。
2. **勘误表全量核对**：逐条比对了 ERR-00…ERR-11 全部 12 份勘误（共 120 条字段纠正），确认每条纠正都已正确反映到 `kRules` 中。
3. **changelog 核对**：通读 v4-to-v5.md、v5-to-v6.md（v1→v2、v3→v4 变更条目为空，v2→v3 仅单位漂移），确认所有 "详见 changelog" 指针字段的取值与 changelog 一致。
4. **proto 字面值核对**：用脚本提取了 v6.proto 全部 48 个绑定结构的适配契约块，逐字段比对——每个字面值字段要么与表一致，要么其差异可由某条勘误精确解释。
5. **8 个公开抽样真值**（stage_state / thrust_reports / thrust_report_v2 / nav_solution_legacy / sep_event_v2 / payload_frame / link_qualities / thermal_sample_patch）与表完全一致。
6. **编译**：`g++ -std=c++17 -Wall -Wextra -Wpedantic` 无警告通过。

仍未覆盖的风险：公开检查只抽样 8 项；其余 40 项依靠上述勘误/changelog/proto 三层交叉核对保证，理论上若某条勘误/changelog 被漏读会出错，但已通过全量逐条比对将该风险降到最低。
