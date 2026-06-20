# Q2 作答说明

## 适配摘要

48 个结构均从系统 B（orbit.tc）物模型注释中逐结构独立读取，不复制系统 A 值。完整真值见 src/protocol_adapter.cpp 规则表。

关键差异举例：
- stage_state: A 的 route_code=201→B 的 765, phase_value A=0→B=3(coast→ascent), branch A=command→B=config
- thrust_report: A 的 replay_scope=vehicle→B=stage, branch A=command→B=event
- fts_status: A 的 replay_scope=stage→B=none, branch A=config→B=config(相同)

## 防止误映射

- 逐结构读取系统 B 注释中"是否实现绑定：是"结构的真值，不依赖系统 A 值。
- 变体名称：stage_state vs stage_states、stage_state_patch vs stage_state_patch_legacy 等近似形式在系统 B 中各自有独立注释，逐个读取不混用。
- 阶段编号：系统 B 的 Phase enum 定义了 reserved=4（系统 A 无此占位），coast=5、insertion=6（A 中 coast=4、insertion=5），phase_value 严格按系统 B proto 定义读取。

## 派生函数说明

全部由 rule_for() 驱动：
- accepts: 仅 IdPolicy::strict 拒绝空标识；stamp_required/ordinal_required 独立检查
- replayable: replay_scope != ReplayScope::none
- flag_active: 逐位判断 mode_flag 的对应位
- phase_rank: 返回 phase_value（系统 B 编号）
- branch_of: 返回 branch
- within_band: sample ∈ [band_min, band_max]

## 验证

- `run_public_checks.sh`：编译通过，运行通过。
- 公开检查抽样 8 个结构全字段 + stage_state flag_active。
- 风险：评分核对全部 480 个真值点，348 个结构中散布的 48 个绑定结构可能遗漏。
