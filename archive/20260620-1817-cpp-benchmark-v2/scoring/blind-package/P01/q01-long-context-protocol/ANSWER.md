# Q1 作答说明：range.tc v6 规则表校正

## 解题结论

仅修改 `src/range_validator.cpp` 中的 `kRules` 数组（48 个实现绑定结构 × 10 个适配维度），将原"未读全语料的错误直觉值"按 ADR-009 优先级交叉核对、校正为 v6 真值。未修改公开头文件、测试与 corpus 任何文件。校验函数（`rule_for/accepts/replayable/flag_active/phase_rank/branch_of/within_band`）一行未动。

构建与验证：`g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread`，`./run_public_checks.sh` 退出码 0（全部 8 个结构锚点 + `flag_active` 抽样断言通过）。

---

## 文档优先级（ADR-009）

严格按以下优先级取舍，高优先级覆盖低优先级：

> **勘误表(errata/ERR-NN) > changelog(vX-to-vY) > v6.proto 适配契约注释 > 字段词典(field_dictionary_v4) > 历史代码(legacy-code)**

冲突解决原则：对任一字段，按上述顺序取"最高优先级文档中给出的最近定义"。勘误表胜出一切——proto / changelog / dict 中与之矛盾的值一律作废。

各来源层级在本答案中用来源键标注：`errata` / `v5v6`(v5-to-v6) / `v4v5`(v4-to-v5) / `v2v3`(v2-to-v3) / `proto`(v6.proto 契约) / `dict`(v4 词典)。v1→v2 与 v3→v4 的 changelog 经核对只含噪声、无结构改动，故不参与。

---

## 陷阱处理（各举实例）

1. **被勘误表推翻的 proto 注释**：`stage_state.route_code` —— proto 契约标注 230，但 ERR 勘误推翻为 576，取 576（这是公开检查的锚点之一，印证勘误优先）。同样 `stage_state.ordinal_required` proto 未置位，勘误明确为必填(true)。

2. **字段词典"已废弃"实为重新启用 / 重新定义**：dict v4 中标记 `*（已废弃/统一）*` 的条目是陷阱——只有当 changelog 重新定义该字段时才采用 changelog 值，否则不得照搬 dict。例如 `payload_frames` 的若干维度，dict 给出旧值，最终以 v5-to-v6 / 勘误为准（如 `route=425` 来自 v5v6，`flag=0x1f` 来自 v4v5）。

3. **单位漂移（ms → 0.1ms）**：v2→v3 频带单位变化，但**数值保持不变**。例如 `stage_state_view` band=(353,784)、`fts_status_patch` band=(2,390)、`payload_frames` band=(277,1162) 均取 v2-to-v3 给出的数值（单位换算不影响实现绑定的整数值）。

4. **枚举重编号（飞行阶段/模式位）**：proto 行中 `v5 枚举重编号(旧值 N), 以重编号后为准` 后跟新字面值——一律采用重编号后的新值。例如阶段值若被重编号行覆盖，取行末新值（`phase_value` 多处来自 v4v5/v5v6 重编号）。

5. **历史代码 / 历史测试的过时值**：legacy-code 与 golden_v3 含已知缺陷，仅作最低优先级参考。未用其任何"直接值"覆盖更高层；仅在其为唯一来源时才采信（实际全部字段均有更高层来源，故 legacy 未成为最终来源）。

6. **近似变体名称混用**：base/plural（如 `stage_state` vs `stage_states`）名称相近但字段不同。逐结构独立读取目标值，禁止跨变体复制。例如 `thrust_report.route=385`(strict/range) 与 `thrust_reports.route=482`(keep_blank/stage) 完全不同。

7. **proto 契约的"本版本起变更"行**：v6.proto 中形如 `本版本起变更, 详见 changelog/vX-to-vY.md` 的行不含字面值，必须跳转到对应 changelog 取值；只有给出完整字面值（如 `route_code: 230`）的行才直接采信。

8. **mode_flag 的位掩码语义**：`flag_active` 用 `mode_flag & (1u << bit)`。如 `stage_state.mode_flag=0x01`，仅 `arm`(bit0) 为 true，其余为 false（公开检查对此专门断言）。

---

## v6 真值表（48 结构 × 10 字段，附来源层）

> 字段顺序：route_code / schema_rev / id_policy / replay_scope / stamp_req / ordinal_req / mode_flag(hex) / phase / branch / band(min,max)。
> 来源键见上"文档优先级"。每字段标注其最终来源层。

### stage 族
- **stage_state**：576[errata] / 2[errata] / auto_gen[v5v6] / vehicle[errata] / F[proto] / T[errata] / 0x01[proto] / 2[v4v5] / command[v5v6] / (191,219)[proto]
- **stage_states**：415[proto] / 2[proto] / strict[errata] / none[v4v5] / T[proto] / F[proto] / 0x0f[v4v5] / 1[proto] / event[proto] / (85,201)[v5v6]
- **stage_state_view**：670[v4v5] / 5[proto] / keep_blank[v4v5] / stage[proto] / T[proto] / F[proto] / 0x35[proto] / 5[v4v5] / command[errata] / (353,784)[v2v3]
- **stage_state_patch**：329[proto] / 5[v4v5] / keep_blank[errata] / none[proto] / T[proto] / T[errata] / 0x2a[v4v5] / 1[proto] / config[proto] / (489,1488)[proto]
- **stage_state_legacy**：482[v5v6] / 9[v4v5] / auto_gen[v4v5] / range[errata] / T[proto] / T[errata] / 0x2c[proto] / 2[v4v5] / event[proto] / (205,1189)[proto]
- **stage_state_v2**：756[proto] / 7[errata] / auto_gen[v4v5] / range[errata] / F[errata] / T[proto] / 0x19[proto] / 2[v4v5] / event[v4v5] / (75,622)[v2v3]

### propulsion 族
- **thrust_report**：385[errata] / 2[v5v6] / strict[proto] / range[v4v5] / F[errata] / F[errata] / 0x06[proto] / 5[v4v5] / command[proto] / (23,834)[proto]
- **thrust_reports**：482[proto] / 2[errata] / keep_blank[errata] / stage[errata] / F[proto] / F[proto] / 0x2f[proto] / 1[proto] / telemetry[v5v6] / (449,916)[v2v3]
- **thrust_report_view**：885[proto] / 9[v4v5] / strict[v4v5] / none[v5v6] / T[proto] / F[proto] / 0x1b[proto] / 2[proto] / event[v4v5] / (71,559)[errata]
- **thrust_report_patch**：465[proto] / 4[proto] / keep_blank[v4v5] / stage[proto] / T[errata] / T[errata] / 0x0f[proto] / 3[v4v5] / command[v4v5] / (459,983)[proto]
- **thrust_report_legacy**：352[proto] / 8[proto] / auto_gen[proto] / none[proto] / F[errata] / F[proto] / 0x08[proto] / 6[v4v5] / event[proto] / (350,545)[proto]
- **thrust_report_v2**：986[errata] / 7[errata] / keep_blank[proto] / vehicle[errata] / T[proto] / T[errata] / 0x04[proto] / 1[proto] / telemetry[v4v5] / (116,519)[proto]

### guidance 族
- **nav_solution**：342[proto] / 4[errata] / strict[v4v5] / none[errata] / F[proto] / T[errata] / 0x3b[v4v5] / 6[v5v6] / event[proto] / (290,612)[v5v6]
- **nav_solutions**：522[proto] / 8[errata] / strict[errata] / range[v4v5] / T[proto] / F[errata] / 0x0e[v4v5] / 6[v4v5] / event[v4v5] / (408,794)[v5v6]
- **nav_solution_view**：892[proto] / 5[proto] / keep_blank[v4v5] / stage[errata] / F[errata] / T[errata] / 0x36[v4v5] / 1[v5v6] / telemetry[v4v5] / (220,728)[errata]
- **nav_solution_patch**：798[v4v5] / 3[v5v6] / auto_gen[proto] / stage[v4v5] / F[errata] / F[errata] / 0x27[proto] / 0[proto] / command[v5v6] / (98,1018)[v4v5]
- **nav_solution_legacy**：565[v5v6] / 8[v4v5] / keep_blank[v4v5] / range[v4v5] / F[proto] / F[errata] / 0x10[proto] / 6[v4v5] / event[v4v5] / (414,872)[proto]
- **nav_solution_v2**：591[proto] / 2[errata] / keep_blank[errata] / vehicle[proto] / T[proto] / F[errata] / 0x04[v4v5] / 0[proto] / event[v4v5] / (351,716)[errata]

### separation 族
- **sep_event**：869[v4v5] / 6[proto] / strict[v4v5] / stage[v4v5] / F[proto] / F[proto] / 0x09[v4v5] / 5[proto] / config[errata] / (464,557)[errata]
- **sep_events**：277[proto] / 8[errata] / keep_blank[errata] / range[errata] / T[proto] / F[errata] / 0x09[v4v5] / 2[v4v5] / event[errata] / (85,931)[proto]
- **sep_event_view**：981[v4v5] / 9[errata] / auto_gen[v4v5] / vehicle[v4v5] / F[errata] / T[proto] / 0x20[proto] / 0[v4v5] / config[v4v5] / (341,1082)[errata]
- **sep_event_patch**：515[proto] / 4[v5v6] / auto_gen[proto] / none[v4v5] / F[proto] / F[errata] / 0x3b[proto] / 0[proto] / config[errata] / (135,814)[proto]
- **sep_event_legacy**：224[proto] / 4[errata] / keep_blank[v5v6] / range[errata] / T[proto] / F[errata] / 0x3a[proto] / 0[proto] / event[v4v5] / (136,432)[v5v6]
- **sep_event_v2**：552[proto] / 6[errata] / strict[v4v5] / vehicle[errata] / F[proto] / F[proto] / 0x1f[proto] / 3[proto] / telemetry[errata] / (42,259)[proto]

### rangesafety 族
- **fts_status**：635[proto] / 6[v5v6] / strict[v4v5] / none[v4v5] / F[errata] / F[proto] / 0x04[v4v5] / 0[proto] / event[v4v5] / (308,432)[errata]
- **fts_statuses**：856[proto] / 3[errata] / strict[v4v5] / vehicle[errata] / T[proto] / F[errata] / 0x16[proto] / 3[v4v5] / telemetry[proto] / (328,549)[proto]
- **fts_status_view**：209[proto] / 2[proto] / keep_blank[v4v5] / vehicle[proto] / F[errata] / T[proto] / 0x14[proto] / 1[proto] / telemetry[errata] / (445,721)[v2v3]
- **fts_status_patch**：825[proto] / 9[proto] / auto_gen[errata] / none[errata] / F[errata] / F[proto] / 0x03[proto] / 0[v4v5] / event[v4v5] / (2,390)[v2v3]
- **fts_status_legacy**：122[proto] / 6[v5v6] / auto_gen[v4v5] / stage[proto] / F[proto] / F[proto] / 0x26[proto] / 1[proto] / event[v4v5] / (209,233)[v2v3]
- **fts_status_v2**：110[v5v6] / 5[proto] / auto_gen[proto] / stage[errata] / F[proto] / F[proto] / 0x30[proto] / 3[proto] / event[v4v5] / (167,454)[proto]

### payload 族
- **payload_frame**：810[proto] / 7[proto] / keep_blank[proto] / vehicle[proto] / T[proto] / F[proto] / 0x33[v4v5] / 5[proto] / telemetry[proto] / (142,766)[proto]
- **payload_frames**：425[v5v6] / 4[v4v5] / strict[v4v5] / range[errata] / F[proto] / F[proto] / 0x1f[v4v5] / 0[proto] / telemetry[proto] / (277,1162)[v2v3]
- **payload_frame_view**：972[errata] / 9[v4v5] / keep_blank[errata] / range[v5v6] / T[proto] / T[errata] / 0x2b[proto] / 3[proto] / event[proto] / (481,1029)[proto]
- **payload_frame_patch**：667[errata] / 7[v4v5] / auto_gen[proto] / range[errata] / F[errata] / T[proto] / 0x11[v4v5] / 0[proto] / command[errata] / (484,812)[errata]
- **payload_frame_legacy**：458[v5v6] / 1[errata] / strict[errata] / range[v4v5] / T[errata] / T[proto] / 0x08[proto] / 1[proto] / event[errata] / (333,700)[proto]
- **payload_frame_v2**：803[v4v5] / 6[errata] / keep_blank[proto] / stage[errata] / F[proto] / T[errata] / 0x1f[proto] / 1[proto] / telemetry[v5v6] / (413,705)[errata]

### groundlink 族
- **link_quality**：412[errata] / 9[proto] / strict[v5v6] / range[v4v5] / T[proto] / F[errata] / 0x39[proto] / 5[proto] / telemetry[errata] / (158,248)[proto]
- **link_qualities**：761[proto] / 9[v4v5] / keep_blank[errata] / range[v4v5] / F[proto] / F[proto] / 0x33[proto] / 5[proto] / command[errata] / (81,926)[v4v5]
- **link_quality_view**：517[proto] / 9[v5v6] / auto_gen[errata] / none[errata] / T[proto] / T[proto] / 0x0c[v4v5] / 3[proto] / command[v4v5] / (122,533)[proto]
- **link_quality_patch**：713[v5v6] / 1[v4v5] / keep_blank[errata] / range[proto] / F[errata] / F[errata] / 0x26[proto] / 5[v5v6] / config[proto] / (390,865)[proto]
- **link_quality_legacy**：782[proto] / 2[v5v6] / keep_blank[v5v6] / none[v5v6] / F[proto] / T[errata] / 0x37[proto] / 5[v4v5] / command[v4v5] / (185,592)[proto]
- **link_quality_v2**：773[proto] / 4[errata] / strict[errata] / range[v5v6] / T[errata] / F[proto] / 0x39[proto] / 1[proto] / event[proto] / (374,931)[errata]

### thermal 族
- **thermal_sample**：100[proto] / 7[errata] / keep_blank[v4v5] / vehicle[proto] / T[proto] / T[errata] / 0x12[v4v5] / 5[v4v5] / event[errata] / (495,883)[proto]
- **thermal_samples**：978[errata] / 6[proto] / auto_gen[proto] / stage[errata] / T[errata] / T[proto] / 0x10[v4v5] / 2[proto] / telemetry[v4v5] / (133,255)[proto]
- **thermal_sample_view**：520[proto] / 9[errata] / keep_blank[errata] / none[errata] / F[errata] / T[errata] / 0x08[proto] / 2[proto] / event[v5v6] / (406,539)[proto]
- **thermal_sample_patch**：197[proto] / 5[v5v6] / strict[v5v6] / vehicle[v4v5] / T[errata] / T[errata] / 0x06[proto] / 0[v4v5] / command[v4v5] / (324,475)[v5v6]
- **thermal_sample_legacy**：130[v5v6] / 9[errata] / keep_blank[v4v5] / range[proto] / F[proto] / T[proto] / 0x34[proto] / 2[proto] / config[v4v5] / (466,1209)[proto]
- **thermal_sample_v2**：819[v4v5] / 1[v5v6] / strict[proto] / none[proto] / T[proto] / F[proto] / 0x20[proto] / 5[v4v5] / config[errata] / (286,669)[proto]

---

## 验证

1. **公开检查（`./run_public_checks.sh`）**：退出码 0，通过。
   - 8 个结构锚点逐字段（route/rev/id_policy/replay_scope/stamp/ordinal/flag/phase/branch/band_min/band_max）全部匹配：stage_state、thrust_reports、thrust_report_v2、nav_solution_legacy、sep_event_v2、payload_frame、link_qualities、thermal_sample_patch。
   - `flag_active` 对 stage_state（mode_flag=0x01）断言：arm=true，ignite/hold/safe/redundant/debug=false，全部通过。
   - `within_band` 边界（含 band_min、band_max，不含 band_min-1、band_max+1）随锚点一并校验。

2. **真值交叉核对（构建期自检）**：在填写前用脚本按 `errata>v5v6>v4v5>v2v3>proto>dict` 优先级组装 48×10 真值表，与 `tests/public_checks.cpp` 的 8 个基准真值逐字段比对，报告"全部 8 个锚点 + 全字段匹配，无缺失字段"。脚本仅用于离线核对，未引入到提交产物中（提交保持纯 C++17、无外部依赖、无 protobuf）。

3. **编译警告**：`-Wall -Wextra -Wpedantic` 下零警告通过。

### 仍未覆盖的风险

- 公开检查只覆盖 8/48 结构的逐字段断言；其余 40 个结构的真值依赖上述按优先级的交叉核对，未逐一写进测试（题目限定不得修改测试）。这些字段同样按 ADR-009 优先级确定，与锚点字段采用同一套取舍规则。
- mode_flag 仅对 stage_state 做了位抽样；其余结构的位语义靠 `mode_flag` 数值本身正确性保证（数值来自上述来源层）。
