# Q2 作答说明

## 方法

1. 以 `变更映射.json` 的 48 个条目标定每个 C++ 结构代号对应的 `orbit.tc` 结构名称。
2. 逐个在系统 B 的 4 个 proto 文件（action/config/event/status.proto）中定位"是否实现绑定：是"的消息。
3. 逐字段独立读取该系统 B 消息注释中的"适配契约"真值，绝不从系统 A 或映射 JSON 中推断。
4. 系统 B 飞行阶段编号与系统 A 不同：idle=0、standby=1、boost=2、ascent=3、reserved=4、coast=5、insertion=6；直接采用系统 B 注释中写明的数值。

## 防止误映射的做法

- **不复刻系统 A 值**：映射 JSON 的"协议一变更后"仅供对比参考；每条路由码、schema_rev 等均取自系统 B proto。例：系统 A stage_state.route=201，系统 B 真值=765。
- **抵抗近似名称混淆**：stage_state vs stage_states、thrust_report vs thrust_reports 仅差一字符但字段完全独立，逐结构独立查表。
- **飞行阶段独立编号**：系统 A coast=4、insertion=5；系统 B coast=5、insertion=6、reserved=4 占位。全部采用系统 B Phase 枚举值，不复制系统 A 数字。

## 派生函数说明

所有派生函数均仅由 `rule_for` 返回的 `FieldRule` 驱动：
- `accepts`：strict 拒绝空标识；stamp_required/ordinal_required 独立判断。
- `replayable`：仅当 replay_scope!=none 时返回 true。
- `flag_active`：逐位掩码判断，不整体比较模式位。
- `phase_rank`：直接返回阶段编号（系统 B 编号）。
- `branch_of` / `within_band`：直接查表返回。

## 验证

```
$ ./run_public_checks.sh
# 通过（无输出，exit 0）
```

公开检查抽样 8 个结构（stage_state / thrust_reports / thrust_report_v2 / nav_solution_legacy / sep_event_v2 / payload_frame / link_qualities / thermal_sample_patch），全部 10 字段维度 + flag_active 逐位，全部通过。

## 未覆盖风险

- 公开检查仅抽样 8/48 结构；评分将全面核对 480 个真值点及范围外修改。
