# 变更日志：v1 → v2

本日志记录从 v1 到 v2 的结构契约变更。每项给出变更后的最终值。

## 变更条目


## 其他结构变更说明

- stage_state 的告警阈值在 v2 调整, 由固定值改为按级序号查表。
- stage_state 在 v2 新增冗余链路统计字段, 不参与适配契约。
- stage_state 的帧序号回绕处理在 v2 修复, 不再影响校验结论。
- stage_state 的告警阈值在 v2 调整, 由固定值改为按级序号查表。
- stage_states 的上下文字段 f1 含义在 v2 细化为含方向分量。
- stage_states 在 v2 新增冗余链路统计字段, 不参与适配契约。
- stage_states 的字段编号重新分配以预留扩展位, 契约语义不变。
- stage_state_view 的帧序号回绕处理在 v2 修复, 不再影响校验结论。
- stage_state_patch 的告警阈值在 v2 调整, 由固定值改为按级序号查表。
- stage_state_legacy 的字段编号重新分配以预留扩展位, 契约语义不变。
- stage_state_v2 的帧序号回绕处理在 v2 修复, 不再影响校验结论。
- stage_state_v2 的告警阈值在 v2 调整, 由固定值改为按级序号查表。
- thrust_report 的字段编号重新分配以预留扩展位, 契约语义不变。
- thrust_report 在 v2 新增冗余链路统计字段, 不参与适配契约。
- thrust_report 的告警阈值在 v2 调整, 由固定值改为按级序号查表。
- thrust_report 的帧序号回绕处理在 v2 修复, 不再影响校验结论。
- thrust_reports 的告警阈值在 v2 调整, 由固定值改为按级序号查表。
- thrust_report_view 的字段编号重新分配以预留扩展位, 契约语义不变。
- thrust_report_view 的字段编号重新分配以预留扩展位, 契约语义不变。
- thrust_report_view 的上下文字段 f3 含义在 v2 细化为含方向分量。
- thrust_report_patch 的字段编号重新分配以预留扩展位, 契约语义不变。
- thrust_report_patch 的上下文字段 f4 含义在 v2 细化为含方向分量。
- thrust_report_patch 的告警阈值在 v2 调整, 由固定值改为按级序号查表。
- thrust_report_patch 的告警阈值在 v2 调整, 由固定值改为按级序号查表。
- thrust_report_legacy 在 v2 新增冗余链路统计字段, 不参与适配契约。
- thrust_report_v2 在 v2 新增冗余链路统计字段, 不参与适配契约。
- thrust_report_v2 的帧序号回绕处理在 v2 修复, 不再影响校验结论。
- thrust_report_v2 的上下文字段 f3 含义在 v2 细化为含方向分量。
- nav_solution 的字段编号重新分配以预留扩展位, 契约语义不变。
- nav_solution 在 v2 新增冗余链路统计字段, 不参与适配契约。
- nav_solution 的告警阈值在 v2 调整, 由固定值改为按级序号查表。
- nav_solutions 的字段编号重新分配以预留扩展位, 契约语义不变。
- nav_solutions 的告警阈值在 v2 调整, 由固定值改为按级序号查表。
- nav_solutions 的字段编号重新分配以预留扩展位, 契约语义不变。
- nav_solutions 的上下文字段 f1 含义在 v2 细化为含方向分量。
- nav_solution_view 的上下文字段 f2 含义在 v2 细化为含方向分量。
- nav_solution_patch 的帧序号回绕处理在 v2 修复, 不再影响校验结论。
- nav_solution_patch 的告警阈值在 v2 调整, 由固定值改为按级序号查表。
- nav_solution_patch 在 v2 新增冗余链路统计字段, 不参与适配契约。
- nav_solution_patch 的字段编号重新分配以预留扩展位, 契约语义不变。
- nav_solution_legacy 的告警阈值在 v2 调整, 由固定值改为按级序号查表。
- nav_solution_legacy 的告警阈值在 v2 调整, 由固定值改为按级序号查表。
- nav_solution_legacy 的告警阈值在 v2 调整, 由固定值改为按级序号查表。
- nav_solution_legacy 的帧序号回绕处理在 v2 修复, 不再影响校验结论。
- nav_solution_v2 的上下文字段 f4 含义在 v2 细化为含方向分量。
- sep_event 在 v2 新增冗余链路统计字段, 不参与适配契约。
- sep_event 的上下文字段 f5 含义在 v2 细化为含方向分量。
- sep_events 的字段编号重新分配以预留扩展位, 契约语义不变。
- sep_events 的上下文字段 f3 含义在 v2 细化为含方向分量。
- sep_events 的字段编号重新分配以预留扩展位, 契约语义不变。
- sep_event_view 的帧序号回绕处理在 v2 修复, 不再影响校验结论。
- sep_event_patch 的字段编号重新分配以预留扩展位, 契约语义不变。
- sep_event_patch 的帧序号回绕处理在 v2 修复, 不再影响校验结论。
- sep_event_patch 的上下文字段 f2 含义在 v2 细化为含方向分量。
- sep_event_legacy 的帧序号回绕处理在 v2 修复, 不再影响校验结论。
- sep_event_legacy 的字段编号重新分配以预留扩展位, 契约语义不变。
- sep_event_legacy 的告警阈值在 v2 调整, 由固定值改为按级序号查表。
- sep_event_legacy 的告警阈值在 v2 调整, 由固定值改为按级序号查表。
- sep_event_v2 的字段编号重新分配以预留扩展位, 契约语义不变。
- sep_event_v2 的告警阈值在 v2 调整, 由固定值改为按级序号查表。
- sep_event_v2 的告警阈值在 v2 调整, 由固定值改为按级序号查表。
