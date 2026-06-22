# 变更日志：v2 → v3

本日志记录从 v2 到 v3 的结构契约变更。每项给出变更后的最终值。

## 变更条目

- **stage_state_view.band**：v2 值为 [353, 784]；v3 起变更为 **[353, 784] (单位 0.1ms; 旧版 ms 须乘 10)**。
- **stage_state_v2.band**：v2 值为 [75, 622]；v3 起变更为 **[75, 622] (单位 0.1ms; 旧版 ms 须乘 10)**。
- **thrust_reports.band**：v2 值为 [449, 916]；v3 起变更为 **[449, 916] (单位 0.1ms; 旧版 ms 须乘 10)**。
- **fts_status_view.band**：v2 值为 [445, 721]；v3 起变更为 **[445, 721] (单位 0.1ms; 旧版 ms 须乘 10)**。
- **fts_status_patch.band**：v2 值为 [2, 390]；v3 起变更为 **[2, 390] (单位 0.1ms; 旧版 ms 须乘 10)**。
- **fts_status_legacy.band**：v2 值为 [209, 233]；v3 起变更为 **[209, 233] (单位 0.1ms; 旧版 ms 须乘 10)**。
- **payload_frames.band**：v2 值为 [277, 1162]；v3 起变更为 **[277, 1162] (单位 0.1ms; 旧版 ms 须乘 10)**。

## 其他结构变更说明

- stage_state 的告警阈值在 v3 调整, 由固定值改为按级序号查表。
- stage_state 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- stage_state 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- stage_state 的字段编号重新分配以预留扩展位, 契约语义不变。
- stage_states 的告警阈值在 v3 调整, 由固定值改为按级序号查表。
- stage_states 的上下文字段 f1 含义在 v3 细化为含方向分量。
- stage_state_view 的告警阈值在 v3 调整, 由固定值改为按级序号查表。
- stage_state_patch 的字段编号重新分配以预留扩展位, 契约语义不变。
- stage_state_patch 的上下文字段 f2 含义在 v3 细化为含方向分量。
- stage_state_legacy 的告警阈值在 v3 调整, 由固定值改为按级序号查表。
- stage_state_legacy 的上下文字段 f4 含义在 v3 细化为含方向分量。
- stage_state_legacy 的字段编号重新分配以预留扩展位, 契约语义不变。
- stage_state_v2 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- stage_state_v2 在 v3 新增冗余链路统计字段, 不参与适配契约。
- thrust_report 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- thrust_report 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- thrust_reports 的字段编号重新分配以预留扩展位, 契约语义不变。
- thrust_reports 在 v3 新增冗余链路统计字段, 不参与适配契约。
- thrust_reports 的上下文字段 f3 含义在 v3 细化为含方向分量。
- thrust_reports 在 v3 新增冗余链路统计字段, 不参与适配契约。
- thrust_report_view 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- thrust_report_view 的告警阈值在 v3 调整, 由固定值改为按级序号查表。
- thrust_report_patch 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- thrust_report_patch 的告警阈值在 v3 调整, 由固定值改为按级序号查表。
- thrust_report_patch 的上下文字段 f5 含义在 v3 细化为含方向分量。
- thrust_report_legacy 的告警阈值在 v3 调整, 由固定值改为按级序号查表。
- thrust_report_legacy 在 v3 新增冗余链路统计字段, 不参与适配契约。
- thrust_report_legacy 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- thrust_report_v2 的上下文字段 f3 含义在 v3 细化为含方向分量。
- nav_solution 在 v3 新增冗余链路统计字段, 不参与适配契约。
- nav_solution 的字段编号重新分配以预留扩展位, 契约语义不变。
- nav_solution 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- nav_solution 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- nav_solutions 的字段编号重新分配以预留扩展位, 契约语义不变。
- nav_solutions 的字段编号重新分配以预留扩展位, 契约语义不变。
- nav_solutions 的字段编号重新分配以预留扩展位, 契约语义不变。
- nav_solutions 的上下文字段 f1 含义在 v3 细化为含方向分量。
- nav_solution_view 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- nav_solution_patch 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- nav_solution_legacy 的告警阈值在 v3 调整, 由固定值改为按级序号查表。
- nav_solution_legacy 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- nav_solution_legacy 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- nav_solution_v2 的告警阈值在 v3 调整, 由固定值改为按级序号查表。
- nav_solution_v2 的告警阈值在 v3 调整, 由固定值改为按级序号查表。
- nav_solution_v2 在 v3 新增冗余链路统计字段, 不参与适配契约。
- sep_event 的告警阈值在 v3 调整, 由固定值改为按级序号查表。
- sep_events 的告警阈值在 v3 调整, 由固定值改为按级序号查表。
- sep_events 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- sep_event_view 的告警阈值在 v3 调整, 由固定值改为按级序号查表。
- sep_event_view 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- sep_event_patch 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- sep_event_patch 在 v3 新增冗余链路统计字段, 不参与适配契约。
- sep_event_patch 的上下文字段 f3 含义在 v3 细化为含方向分量。
- sep_event_legacy 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- sep_event_legacy 的上下文字段 f4 含义在 v3 细化为含方向分量。
- sep_event_v2 的帧序号回绕处理在 v3 修复, 不再影响校验结论。
- sep_event_v2 在 v3 新增冗余链路统计字段, 不参与适配契约。
- sep_event_v2 的字段编号重新分配以预留扩展位, 契约语义不变。
