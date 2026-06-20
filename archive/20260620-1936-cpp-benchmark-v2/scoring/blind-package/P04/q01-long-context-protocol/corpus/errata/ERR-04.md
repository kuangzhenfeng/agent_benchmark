# 勘误 ERR-04

本勘误推翻既有文档(v6.proto 注释或 changelog)中的下列值。勘误表优先级最高。

## 纠正条目

- **thrust_report.ordinal_required**：原文档记为 true，应更正为 **false**。
- **nav_solution_v2.band**：原文档记为 [186, 123]，应更正为 **[351, 716]**。
- **sep_event.band**：原文档记为 [98, 345]，应更正为 **[464, 557]**。
- **sep_events.ordinal_required**：原文档记为 true，应更正为 **false**。
- **sep_event_view.band**：原文档记为 [414, 339]，应更正为 **[341, 1082]**。
- **sep_event_patch.branch**：原文档记为 指令，应更正为 **配置**。
- **payload_frame_view.route_code**：原文档记为 175，应更正为 **972**。
- **link_quality_v2.band**：原文档记为 [27, 160]，应更正为 **[374, 931]**。
- **thermal_sample.ordinal_required**：原文档记为 false，应更正为 **true**。

## 背景

- stage_state 的上下文字段 f4 含义在 v6 细化为含方向分量。
- stage_state 的上下文字段 f3 含义在 v6 细化为含方向分量。
- stage_states 的告警阈值在 v6 调整, 由固定值改为按级序号查表。
- stage_states 在 v6 新增冗余链路统计字段, 不参与适配契约。
- stage_states 在 v6 新增冗余链路统计字段, 不参与适配契约。
- stage_states 的告警阈值在 v6 调整, 由固定值改为按级序号查表。
- stage_state_view 在 v6 新增冗余链路统计字段, 不参与适配契约。
- stage_state_view 的帧序号回绕处理在 v6 修复, 不再影响校验结论。
- stage_state_patch 的字段编号重新分配以预留扩展位, 契约语义不变。
- stage_state_patch 的字段编号重新分配以预留扩展位, 契约语义不变。
- stage_state_legacy 的告警阈值在 v6 调整, 由固定值改为按级序号查表。
- stage_state_legacy 的上下文字段 f4 含义在 v6 细化为含方向分量。
- stage_state_v2 的字段编号重新分配以预留扩展位, 契约语义不变。
- thrust_report 的上下文字段 f4 含义在 v6 细化为含方向分量。
- thrust_report 在 v6 新增冗余链路统计字段, 不参与适配契约。
- thrust_report 的字段编号重新分配以预留扩展位, 契约语义不变。
- thrust_report 的上下文字段 f5 含义在 v6 细化为含方向分量。
- thrust_reports 的上下文字段 f2 含义在 v6 细化为含方向分量。
- thrust_reports 的告警阈值在 v6 调整, 由固定值改为按级序号查表。
- thrust_report_view 的帧序号回绕处理在 v6 修复, 不再影响校验结论。
- thrust_report_view 在 v6 新增冗余链路统计字段, 不参与适配契约。
- thrust_report_view 的字段编号重新分配以预留扩展位, 契约语义不变。
- thrust_report_patch 的告警阈值在 v6 调整, 由固定值改为按级序号查表。
- thrust_report_legacy 在 v6 新增冗余链路统计字段, 不参与适配契约。
- thrust_report_v2 的告警阈值在 v6 调整, 由固定值改为按级序号查表。
- thrust_report_v2 的帧序号回绕处理在 v6 修复, 不再影响校验结论。
- thrust_report_v2 在 v6 新增冗余链路统计字段, 不参与适配契约。
- thrust_report_v2 的字段编号重新分配以预留扩展位, 契约语义不变。
- nav_solution 的上下文字段 f1 含义在 v6 细化为含方向分量。
- nav_solutions 在 v6 新增冗余链路统计字段, 不参与适配契约。
- nav_solutions 的帧序号回绕处理在 v6 修复, 不再影响校验结论。
- nav_solutions 的上下文字段 f5 含义在 v6 细化为含方向分量。
- nav_solutions 的字段编号重新分配以预留扩展位, 契约语义不变。
- nav_solution_view 的帧序号回绕处理在 v6 修复, 不再影响校验结论。
- nav_solution_view 的帧序号回绕处理在 v6 修复, 不再影响校验结论。
- nav_solution_patch 的字段编号重新分配以预留扩展位, 契约语义不变。
- nav_solution_patch 的上下文字段 f3 含义在 v6 细化为含方向分量。
- nav_solution_legacy 的字段编号重新分配以预留扩展位, 契约语义不变。
- nav_solution_legacy 的帧序号回绕处理在 v6 修复, 不再影响校验结论。
- nav_solution_legacy 的上下文字段 f5 含义在 v6 细化为含方向分量。
- nav_solution_legacy 的上下文字段 f1 含义在 v6 细化为含方向分量。
- nav_solution_v2 的上下文字段 f2 含义在 v6 细化为含方向分量。
- nav_solution_v2 在 v6 新增冗余链路统计字段, 不参与适配契约。
- nav_solution_v2 的字段编号重新分配以预留扩展位, 契约语义不变。
- sep_event 的帧序号回绕处理在 v6 修复, 不再影响校验结论。
- sep_event 的帧序号回绕处理在 v6 修复, 不再影响校验结论。
- sep_event 的告警阈值在 v6 调整, 由固定值改为按级序号查表。
- sep_event 的上下文字段 f4 含义在 v6 细化为含方向分量。
- sep_events 的告警阈值在 v6 调整, 由固定值改为按级序号查表。
- sep_events 的帧序号回绕处理在 v6 修复, 不再影响校验结论。
- sep_events 的帧序号回绕处理在 v6 修复, 不再影响校验结论。
- sep_events 的告警阈值在 v6 调整, 由固定值改为按级序号查表。
- sep_event_view 的告警阈值在 v6 调整, 由固定值改为按级序号查表。
- sep_event_view 的帧序号回绕处理在 v6 修复, 不再影响校验结论。
- sep_event_view 的上下文字段 f5 含义在 v6 细化为含方向分量。
- sep_event_view 在 v6 新增冗余链路统计字段, 不参与适配契约。
- sep_event_patch 的告警阈值在 v6 调整, 由固定值改为按级序号查表。
- sep_event_patch 的上下文字段 f1 含义在 v6 细化为含方向分量。
- sep_event_legacy 的上下文字段 f2 含义在 v6 细化为含方向分量。
- sep_event_legacy 的上下文字段 f2 含义在 v6 细化为含方向分量。
- sep_event_legacy 的帧序号回绕处理在 v6 修复, 不再影响校验结论。
- sep_event_v2 的上下文字段 f5 含义在 v6 细化为含方向分量。
