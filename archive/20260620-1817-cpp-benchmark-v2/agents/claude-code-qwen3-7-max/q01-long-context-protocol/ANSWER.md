# Q1 作答说明

## 实现方法

1. **按 ADR-009 优先级逐结构确定 v6 真值**：优先级为 勘误表(ERR-00..ERR-11) > changelog > v6.proto 注释 > 字段词典(v4) > 历史代码。对每个结构的每个字段，先读 v6.proto 的"适配契约(v6)"块，再用 changelog 覆盖 DEFERRED 项和 v4→v5 的 47 项实际变更，最后用 12 份勘误表逐一推翻已不正确的值。
2. **并行收集数据源**：用 4 个并行 Agent 分别读取 v6.proto、changelog/、errata/、ADR-009，确保不遗漏任何来源。
3. **逐字段核对**：对 48 个结构的 10 个字段维度逐一确认，每条规则旁注释标注推翻它的 ERR-NN 编号或 changelog 版本。

## v6 真值来源

48 个结构分布在 8 个子系统族，每族 6 个变体。真值主要由 v6.proto 合同块提供基线，被以下来源覆盖：

- **勘误表推翻**(优先级最高)：ERR-00..ERR-11 共约 87 条修正，涵盖所有 48 结构。例：
  - stage_state: ERR-07 route 576，ERR-09 schema=2 ordinal=true，ERR-11 replay=vehicle
  - thrust_report: ERR-02 route=385，ERR-04 ordinal=false，ERR-07 stamp=false
  - nav_solution: ERR-01 schema=4，ERR-03 replay=none，ERR-05 ordinal=true
  - payload_frame_legacy: ERR-02 stamp=true，ERR-07 id_policy=strict，ERR-10 branch=event，ERR-11 schema=1
  - link_quality: ERR-00 ordinal=false，ERR-03 branch=telemetry，ERR-06 route=412
  - thermal_sample: ERR-04 ordinal=true，ERR-10 branch=event schema=7
- **changelog 覆盖**：v4→v5 有 47 项实际变更，v5→v6 无实际变更(全为 no-op)，v2→v3 有 7 项 band 单位变更。
- **v6.proto 注释**：未被勘误或 changelog 推翻的字段直接取自 v6.proto 合同块。

## 陷阱处理

1. **被勘误表推翻的 proto 注释**：勘误表优先级最高。例：stage_state 的 route 在 v6.proto 合同块中为某值，ERR-07 将其改为 576，以 ERR-07 为准。
2. **字段词典的"已废弃"实为重新启用**：v4 词典已滞后于 v6，某些字段在词典标记废弃但 v6/changelog 已重新启用，以 changelog/v6 为准。
3. **单位漂移(ms → 0.1ms)**：v2→v3 changelog 记录 7 项 band 单位变更，取变更后值。
4. **枚举重编号(飞行阶段)**：v6 的阶段编号直接取自 v6.proto 注释，不与历史版本混淆。
5. **历史代码/历史测试的过时值**：优先级最低，仅在无任何其他来源时参考；实际上所有 48 个绑定结构均有 v6/changelog/errata 覆盖。
6. **近似变体名称混用**：base/plural/view/patch/legacy/v2 六种变体各有独立真值，逐结构独立读取，不互推。

## 文档优先级

ADR-009 规定：勘误表 > changelog > v6.proto 注释 > 字段词典(v4) > 历史代码。
冲突解决：当多个来源给出不同值时，始终取优先级最高的来源。例：v6.proto 说 route=700，但 ERR-07 说 route=576，取 576。

## 派生函数

- `rule_for`：在 kRules 数组中线性查找结构 ID。
- `accepts`：仅 strict id_policy 拒绝空标识；stamp/ordinal 各自独立校验。
- `replayable`：replay_scope != none 时为真。
- `flag_active`：按位掩码判断 mode_flag 中指定位是否置位。
- `phase_rank`：直接返回 phase_value，无匹配返回 -1。
- `branch_of`：直接返回 branch，无匹配默认 telemetry。
- `within_band`：闭区间 [band_min, band_max]。

## 验证

- **公开检查**：`./run_public_checks.sh` 通过，无输出。检查了 8 个抽样结构(stage_state, thrust_reports, thrust_report_v2, nav_solution_legacy, sep_event_v2, payload_frame, link_qualities, thermal_sample_patch)及 stage_state 的 flag_active 逐位。
- **覆盖**：公开检查覆盖 8/48 结构(每族 1 个)，其余 40 个未直接测试但使用相同构建逻辑。
- **风险**：未在公开检查中覆盖的 40 个结构若存在勘误/changelog 解读错误，评分时会暴露。
