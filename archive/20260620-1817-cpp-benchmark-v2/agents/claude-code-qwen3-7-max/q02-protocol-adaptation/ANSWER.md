# Q2 作答说明

## 适配摘要

仅修改 `src/protocol_adapter.cpp`，将系统 A (range.tc) 的 48 个实现绑定结构适配为系统 B (orbit.tc) 的真值。所有字段值独立读取自系统 B 的 proto 文件注释。

### 系统 B 真值来源

- **status.proto**：stage 族(6) + fts 族(6) = 12 结构
- **action.proto**：thrust 族(6) + payload 族(6) = 12 结构
- **config.proto**：nav 族(6) + link 族(6) = 12 结构
- **event.proto**：sep 族(6) + thermal 族(6) = 12 结构

### 关键字段映射

| 字段维度 | 系统 B 取值来源 |
|---------|----------------|
| route_code | 结构注释中"路由码" |
| schema_rev | 结构注释中"协议小版本" |
| id_policy | 结构注释中"标识策略"(严格必填/保留空值/空则生成) |
| replay_scope | 结构注释中"回放域"(不回放/本级/本箭/本靶区) |
| stamp_required | 结构注释中"时间戳必填" |
| ordinal_required | 结构注释中"序号必填" |
| mode_flag | 结构注释中"模式位"六位掩码 |
| phase_value | 结构注释中"飞行阶段编号"(注意系统 B 编号与 A 不同) |
| branch | 结构注释中"载荷分支"(遥测/指令/配置/事件) |
| band_min/max | 结构注释中"合法区间" |

### 阶段编号差异

系统 B 飞行阶段编号与系统 A 不同：
- idle=0, standby=1, boost=2, ascent=3, **reserved=4**, coast=5, insertion=6
- 系统 A 中 coast=4, insertion=5；系统 B 的 4 号位是 reserved 占位

## 防止误映射的做法

1. **不复制系统 A 值**：变更映射.json 中列出了系统 A 的完成值，但仅作参考，不直接复制到实现。每个字段独立从系统 B proto 注释读取。
2. **区分变体名称**：base/plural/view/patch/legacy/v2 六种变体各自独立，逐结构查找对应注释块，不因名称相似而混用。
3. **阶段编号独立读取**：系统 B 的 phase_value 直接从其 proto 注释读取，不使用系统 A 的编号体系。例：系统 A coast=4，系统 B coast=5，以系统 B 为准。
4. **逐字段核对**：10 个字段维度互相独立，不从一个字段推断另一个字段。

## 派生函数说明

所有派生函数由 `rule_for` 驱动，查询 kRules 规则表：

- **accepts(id, in)**：若 id_policy==strict 且 identifier 为空，拒绝；若 stamp_required 且无时间戳，拒绝；若 ordinal_required 且无序号，拒绝；否则接受。
- **replayable(id)**：若 replay_scope != none，返回 true。
- **flag_active(id, bit)**：用位掩码 `1 << bit` 测试 mode_flag 中指定位是否置位。
- **phase_rank(id)**：返回 phase_value 整数值，无匹配返回 -1。
- **branch_of(id)**：返回 branch 枚举值，无匹配默认 telemetry。
- **within_band(id, sample)**：判断 sample 是否在 [band_min, band_max] 闭区间内。

## 验证

- **公开检查**：`./run_public_checks.sh` 通过，无输出。检查了 8 个抽样结构：
  - stage_state (route=765, schema=5, keep_blank, vehicle, stamp=true, ordinal=true, mode=11, phase=3, config, [313,925])
  - thrust_reports (521, 2, strict, none, true, false, 48, 0, event, [202,452])
  - thrust_report_v2 (453, 2, keep_blank, vehicle, true, false, 51, 5, config, [89,811])
  - nav_solution_legacy (617, 2, keep_blank, none, true, true, 54, 1, command, [241,796])
  - sep_event_v2 (644, 6, strict, stage, true, false, 43, 1, event, [482,1430])
  - payload_frame (108, 5, auto_gen, vehicle, false, true, 45, 5, telemetry, [363,994])
  - link_qualities (683, 6, keep_blank, vehicle, false, false, 1, 2, config, [471,791])
  - thermal_sample_patch (821, 7, auto_gen, none, true, true, 2, 3, telemetry, [447,1033])
- **覆盖**：公开检查覆盖 8/48 结构(每族 1 个)，其余 40 个使用相同构建逻辑但未直接测试。
- **风险**：未在公开检查中覆盖的 40 个结构若存在系统 B proto 注释解读错误，评分时会暴露。
