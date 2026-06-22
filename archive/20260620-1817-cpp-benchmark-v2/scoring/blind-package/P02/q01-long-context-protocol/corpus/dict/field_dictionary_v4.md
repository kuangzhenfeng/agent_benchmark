# 字段词典 range.tc v4

> 本词典停留在 v4。部分字段已废弃或被后续 changelog 重新定义。本词典**不可**作为 v6 的唯一真值来源。

## 词条

### stage_state

- id_policy: 空则生成
- stamp_required: false

### stage_states

- schema_rev: 2
- id_policy: 严格必填
- replay_scope: range  *（已废弃/统一）*
- ordinal_required: false
- branch: 事件
- band: [85, 201]

### stage_state_view

- schema_rev: 5
- mode_flag: 53 (arm=1 ignite=0 hold=1 safe=0 redundant=1 debug=1)
- phase_value: 5
- branch: 指令

### stage_state_patch

- schema_rev: 5
- id_policy: 保留空值
- stamp_required: true
- ordinal_required: true
- mode_flag: 42 (arm=0 ignite=1 hold=0 safe=1 redundant=0 debug=1)
- phase_value: 1

### stage_state_legacy

- stamp_required: true
- ordinal_required: true
- phase_value: 2
- branch: 事件

### stage_state_v2

- route_code: 756
- schema_rev: 7
- mode_flag: 25 (arm=1 ignite=0 hold=0 safe=1 redundant=1 debug=0)
- phase_value: 2
- branch: 事件
- band: [75, 622]

### thrust_report

- schema_rev: 2
- id_policy: 严格必填
- replay_scope: vehicle  *（已废弃/统一）*
- stamp_required: false
- ordinal_required: false
- mode_flag: 6 (arm=0 ignite=1 hold=1 safe=0 redundant=0 debug=0)
- branch: 指令

### thrust_reports

- route_code: 482
- mode_flag: 47 (arm=1 ignite=1 hold=1 safe=1 redundant=0 debug=1)
- phase_value: 1
- branch: 遥测
- band: [449, 916]

### thrust_report_view

- id_policy: keep_blank  *（已废弃/统一）*
- replay_scope: 不回放
- stamp_required: true
- branch: telemetry  *（已废弃/统一）*

### thrust_report_patch

- id_policy: auto_gen  *（已废弃/统一）*
- stamp_required: true
- ordinal_required: true
- phase_value: 3
- branch: event  *（已废弃/统一）*
- band: [459, 983]

### thrust_report_legacy

- schema_rev: 8
- replay_scope: 不回放
- stamp_required: false
- mode_flag: 8 (arm=0 ignite=0 hold=0 safe=1 redundant=0 debug=0)
- phase_value: 6
- branch: 事件
- band: [350, 545]

### thrust_report_v2

- route_code: 986
- id_policy: 保留空值
- mode_flag: 4 (arm=0 ignite=0 hold=1 safe=0 redundant=0 debug=0)
- phase_value: 1
- branch: 遥测
- band: [116, 519]

### nav_solution

- route_code: 342
- schema_rev: 4
- id_policy: keep_blank  *（已废弃/统一）*
- ordinal_required: true
- mode_flag: 59 (arm=1 ignite=1 hold=0 safe=1 redundant=1 debug=1)
- phase_value: 6
- branch: 事件
- band: [290, 612]

### nav_solutions

- stamp_required: true
- ordinal_required: false
- phase_value: 6
- branch: 事件
- band: [408, 794]

### nav_solution_view

- route_code: 892
- schema_rev: 5
- replay_scope: 本级
- stamp_required: false
- ordinal_required: true

### nav_solution_patch

- replay_scope: none  *（已废弃/统一）*
- ordinal_required: false
- mode_flag: 39 (arm=1 ignite=1 hold=1 safe=0 redundant=0 debug=1)
- phase_value: 0
- band: [98, 1018]

### nav_solution_legacy

- route_code: 565
- id_policy: auto_gen  *（已废弃/统一）*
- stamp_required: false
- ordinal_required: false

### nav_solution_v2

- id_policy: 保留空值
- replay_scope: 本箭
- ordinal_required: false
- phase_value: 0
- branch: 事件
- band: [351, 716]

### sep_event

- stamp_required: false
- ordinal_required: false
- phase_value: 5
- branch: 配置
- band: [464, 557]

### sep_events

- schema_rev: 8
- id_policy: 保留空值
- replay_scope: 本靶区
- ordinal_required: false
- mode_flag: 9 (arm=1 ignite=0 hold=0 safe=1 redundant=0 debug=0)
- branch: 事件

### sep_event_view

- route_code: 981
- id_policy: 空则生成
- replay_scope: 本箭
- ordinal_required: true
- mode_flag: 32 (arm=0 ignite=0 hold=0 safe=0 redundant=0 debug=1)
- branch: telemetry  *（已废弃/统一）*

### sep_event_patch

- route_code: 515
- schema_rev: 4
- ordinal_required: false
- phase_value: 0
- band: [135, 814]

### sep_event_legacy

- route_code: 224
- id_policy: 保留空值
- replay_scope: 本靶区
- stamp_required: true
- phase_value: 0
- branch: telemetry  *（已废弃/统一）*

### sep_event_v2

- route_code: 552
- id_policy: auto_gen  *（已废弃/统一）*
- stamp_required: false
- mode_flag: 31 (arm=1 ignite=1 hold=1 safe=1 redundant=1 debug=0)
- phase_value: 3
- branch: 遥测

### fts_status

- id_policy: auto_gen  *（已废弃/统一）*
- stamp_required: false
- ordinal_required: false
- mode_flag: 4 (arm=0 ignite=0 hold=1 safe=0 redundant=0 debug=0)
- phase_value: 0
- branch: 事件
- band: [308, 432]

### fts_statuses

- id_policy: 严格必填
- stamp_required: true
- phase_value: 3
- band: [328, 549]

### fts_status_view

- id_policy: 保留空值
- stamp_required: false
- phase_value: 1
- band: [445, 721]

### fts_status_patch

- route_code: 825
- schema_rev: 9
- id_policy: 空则生成
- replay_scope: 不回放
- ordinal_required: false
- mode_flag: 3 (arm=1 ignite=1 hold=0 safe=0 redundant=0 debug=0)
- branch: 事件

### fts_status_legacy

- route_code: 122
- schema_rev: 6
- id_policy: strict  *（已废弃/统一）*
- replay_scope: 本级
- ordinal_required: false
- mode_flag: 38 (arm=0 ignite=1 hold=1 safe=0 redundant=0 debug=1)
- branch: 事件
- band: [209, 233]

### fts_status_v2

- route_code: 110
- id_policy: 空则生成
- stamp_required: false
- ordinal_required: false
- branch: config  *（已废弃/统一）*

### payload_frame

- id_policy: 保留空值
- stamp_required: true
- ordinal_required: false
- mode_flag: 51 (arm=1 ignite=1 hold=0 safe=0 redundant=1 debug=1)
- phase_value: 5

### payload_frames

- schema_rev: 4
- id_policy: keep_blank  *（已废弃/统一）*
- stamp_required: false
- ordinal_required: false
- band: [277, 1162]

### payload_frame_view

- route_code: 972
- id_policy: 保留空值
- ordinal_required: true

### payload_frame_patch

- mode_flag: 17 (arm=1 ignite=0 hold=0 safe=0 redundant=1 debug=0)
- phase_value: 0
- branch: 指令
- band: [484, 812]

### payload_frame_legacy

- id_policy: 严格必填
- replay_scope: stage  *（已废弃/统一）*
- stamp_required: true
- phase_value: 1
- branch: 事件
- band: [333, 700]

### payload_frame_v2

- route_code: 803
- id_policy: 保留空值
- ordinal_required: true
- mode_flag: 31 (arm=1 ignite=1 hold=1 safe=1 redundant=1 debug=0)
- branch: 遥测

### link_quality

- replay_scope: 本靶区
- ordinal_required: false

### link_qualities

- schema_rev: 9
- replay_scope: none  *（已废弃/统一）*
- mode_flag: 51 (arm=1 ignite=1 hold=0 safe=0 redundant=1 debug=1)
- band: [81, 926]

### link_quality_view

- route_code: 517
- id_policy: 空则生成
- replay_scope: 不回放
- stamp_required: true
- ordinal_required: true
- branch: telemetry  *（已废弃/统一）*
- band: [122, 533]

### link_quality_patch

- route_code: 713
- replay_scope: 本靶区
- ordinal_required: false
- mode_flag: 38 (arm=0 ignite=1 hold=1 safe=0 redundant=0 debug=1)

### link_quality_legacy

- schema_rev: 2
- id_policy: 保留空值
- stamp_required: false
- ordinal_required: true
- mode_flag: 55 (arm=1 ignite=1 hold=1 safe=0 redundant=1 debug=1)
- band: [185, 592]

### link_quality_v2

- route_code: 773
- id_policy: 严格必填
- phase_value: 1
- branch: 事件
- band: [374, 931]

### thermal_sample

- route_code: 100
- schema_rev: 7
- id_policy: 保留空值
- ordinal_required: true
- mode_flag: 18 (arm=0 ignite=1 hold=0 safe=0 redundant=1 debug=0)
- branch: 事件
- band: [495, 883]

### thermal_samples

- route_code: 978
- schema_rev: 6
- id_policy: 空则生成
- band: [133, 255]

### thermal_sample_view

- route_code: 520
- schema_rev: 9
- id_policy: 保留空值
- stamp_required: false
- mode_flag: 8 (arm=0 ignite=0 hold=0 safe=1 redundant=0 debug=0)

### thermal_sample_patch

- route_code: 197
- id_policy: 严格必填
- replay_scope: range  *（已废弃/统一）*
- ordinal_required: true
- mode_flag: 6 (arm=0 ignite=1 hold=1 safe=0 redundant=0 debug=0)
- phase_value: 0
- branch: 指令
- band: [324, 475]

### thermal_sample_legacy

- route_code: 130
- id_policy: auto_gen  *（已废弃/统一）*
- ordinal_required: true
- phase_value: 2
- branch: command  *（已废弃/统一）*

### thermal_sample_v2

- id_policy: 严格必填
- ordinal_required: false
- phase_value: 5
- branch: 配置

