# Q1 作答说明

## 文档优先级

按 ADR-009：**勘误表 > changelog > v6.proto 注释 > 字段词典(v4) > 历史代码**

每字段先取 v6.proto 注释值，再按 changelog 链式追溯（v5→v6、v4→v5、v3→v4、v2→v3），最后以勘误表(ERR-00~ERR-11)为最高权威覆盖冲突值。

## 陷阱处理

- **勘误推翻 proto**：如 stage_state_patch.id_policy proto 写"严格必填"，ERR-00 改为"保留空值"；fts_status.band proto 写 [110,285]，ERR-02 改为 [308,432]。
- **词典"已废弃"实为重新启用**：如 thermal_sample_legacy.id_policy 词典标"已废弃"，但 changelog v4→v5 记为"保留空值"，ERR-09 无推翻，最终取 keep_blank。
- **单位漂移(ms→0.1ms)**：band 注释"单位自 v3 起变更"的结构（如 fts_status_view、thrust_reports），从 v2→v3 changelog 取得 [445,721] 等值，数值本身不变但单位已切换。
- **枚举重编号(v5)**：v6.proto Phase 新增 RESERVED=4，旧 coast=4→5，insertion=5→6。proto 注释对 mode_flag/phase_value 多给出两行（旧值→新值），取新值。
- **历史代码/测试过时值**：如 validator_v4.cpp 中 thrust_report_v2.replay_scope=stage，但 ERR-03 勘误改为 vehicle；golden_v3.cpp 预期值属 v3 时代，已不适用 v6。
- **近似变体混用**：如 stage_state_view vs stage_state_view_patch（proto 中无 patch 变体），或 stage_states vs stage_state（id_policy 不同），逐结构独立读取。

## 验证

- `run_public_checks.sh`：编译通过，运行通过（无 assert 失败）。
- 公开检查抽样了 8 个结构的全部字段 + stage_state 的 flag_active 6 位。
- 风险：评分会逐字段核对全部 480 个真值点，部分 proto 注释/changelog 链式引用可能存在遗漏。
