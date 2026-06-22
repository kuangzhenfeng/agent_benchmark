# Q2 作答说明

## 适配摘要

缩写：`strict/keep/auto` 分别为严格必填、保留空值、空则生成；`none/stage/vehicle/range` 分别为不回放、本级、本箭、本靶区；`T/O` 为时间戳/序号。

| 结构 | route | rev | id | replay | T/O | mode | phase | branch | band |
|---|---:|---:|---|---|---|---:|---:|---|---|
| stage_state | 765 | 5 | keep | vehicle | T/T | 0x0b | 3 | config | [313,925] |
| stage_states | 924 | 7 | keep | none | T/T | 0x1d | 0 | event | [295,1082] |
| stage_state_view | 960 | 7 | keep | range | F/T | 0x3e | 2 | command | [348,777] |
| stage_state_patch | 694 | 5 | auto | vehicle | F/T | 0x02 | 0 | event | [54,739] |
| stage_state_legacy | 105 | 7 | keep | vehicle | T/F | 0x21 | 1 | event | [389,498] |
| stage_state_v2 | 421 | 5 | keep | vehicle | T/F | 0x17 | 3 | telemetry | [0,112] |
| thrust_report | 709 | 8 | keep | stage | T/T | 0x01 | 1 | event | [362,486] |
| thrust_reports | 521 | 2 | strict | none | T/F | 0x30 | 0 | event | [202,452] |
| thrust_report_view | 389 | 8 | strict | vehicle | T/F | 0x21 | 5 | event | [30,435] |
| thrust_report_patch | 216 | 7 | keep | vehicle | F/F | 0x10 | 0 | config | [333,1120] |
| thrust_report_legacy | 242 | 9 | auto | vehicle | T/F | 0x0e | 6 | config | [196,864] |
| thrust_report_v2 | 453 | 2 | keep | vehicle | T/F | 0x33 | 5 | config | [89,811] |
| nav_solution | 700 | 2 | auto | none | T/F | 0x10 | 1 | command | [115,1083] |
| nav_solutions | 583 | 5 | keep | none | T/T | 0x0a | 0 | command | [158,406] |
| nav_solution_view | 147 | 8 | keep | none | F/F | 0x06 | 1 | command | [67,738] |
| nav_solution_patch | 838 | 2 | keep | none | F/T | 0x03 | 5 | event | [419,805] |
| nav_solution_legacy | 617 | 2 | keep | none | T/T | 0x36 | 1 | command | [241,796] |
| nav_solution_v2 | 701 | 8 | keep | none | T/T | 0x0b | 0 | command | [237,785] |
| sep_event | 786 | 5 | auto | range | F/T | 0x1a | 6 | command | [321,610] |
| sep_events | 625 | 8 | auto | range | T/F | 0x3b | 1 | command | [194,1044] |
| sep_event_view | 867 | 6 | auto | none | T/F | 0x3d | 2 | command | [253,1129] |
| sep_event_patch | 601 | 5 | keep | none | T/T | 0x1e | 2 | config | [31,311] |
| sep_event_legacy | 427 | 4 | keep | vehicle | F/F | 0x23 | 3 | telemetry | [381,1049] |
| sep_event_v2 | 644 | 6 | strict | stage | T/F | 0x2b | 1 | event | [482,1430] |
| fts_status | 103 | 7 | strict | none | F/T | 0x1a | 0 | config | [25,714] |
| fts_statuses | 289 | 3 | auto | none | T/F | 0x35 | 5 | event | [17,885] |
| fts_status_view | 915 | 4 | auto | stage | F/T | 0x22 | 2 | telemetry | [223,497] |
| fts_status_patch | 809 | 9 | keep | none | F/T | 0x07 | 5 | command | [389,955] |
| fts_status_legacy | 476 | 5 | auto | vehicle | T/T | 0x0f | 2 | config | [103,1073] |
| fts_status_v2 | 429 | 5 | auto | vehicle | T/T | 0x22 | 1 | event | [19,159] |
| payload_frame | 108 | 5 | auto | vehicle | F/T | 0x2d | 5 | telemetry | [363,994] |
| payload_frames | 593 | 7 | auto | range | T/T | 0x04 | 1 | config | [419,832] |
| payload_frame_view | 175 | 9 | strict | vehicle | T/T | 0x03 | 0 | telemetry | [99,880] |
| payload_frame_patch | 697 | 5 | keep | none | T/T | 0x21 | 3 | command | [153,173] |
| payload_frame_legacy | 792 | 5 | keep | stage | T/F | 0x03 | 1 | telemetry | [202,630] |
| payload_frame_v2 | 255 | 1 | strict | range | F/T | 0x1f | 0 | event | [36,463] |
| link_quality | 239 | 7 | strict | range | T/T | 0x28 | 1 | config | [125,249] |
| link_qualities | 683 | 6 | keep | vehicle | F/F | 0x01 | 2 | config | [471,791] |
| link_quality_view | 509 | 9 | strict | range | F/T | 0x03 | 0 | command | [136,428] |
| link_quality_patch | 386 | 8 | auto | none | T/T | 0x2e | 2 | config | [220,302] |
| link_quality_legacy | 424 | 8 | keep | vehicle | T/T | 0x3d | 5 | telemetry | [277,1022] |
| link_quality_v2 | 923 | 1 | strict | stage | T/T | 0x3b | 0 | config | [303,1110] |
| thermal_sample | 454 | 2 | strict | range | F/T | 0x17 | 1 | command | [302,608] |
| thermal_samples | 127 | 5 | keep | vehicle | T/F | 0x3c | 5 | telemetry | [214,613] |
| thermal_sample_view | 716 | 3 | auto | range | T/F | 0x3b | 0 | config | [330,629] |
| thermal_sample_patch | 821 | 7 | auto | none | T/T | 0x02 | 3 | telemetry | [447,1033] |
| thermal_sample_legacy | 749 | 4 | strict | none | F/F | 0x1a | 3 | event | [494,908] |
| thermal_sample_v2 | 316 | 3 | strict | stage | F/T | 0x02 | 2 | event | [121,234] |

## 防止误映射的做法

以 `系统A/变更映射.json` 的精确 C++ 结构代号定位候选项，再只接受系统 B 中同时满足“是否实现绑定: 是”和同名 `message` 的卡片。检查时要求注释的结构代号与 `message` 名称完全一致，并拒绝重复项和非绑定结构。

系统 A 仅用来定位映射，未读取其值作为字段来源。每个字段均从系统 B 卡片独立解析，因此没有按单复数、`view`、`patch`、`legacy` 或 `v2` 名称推导。阶段值直接取 B 卡片中的数字，使用 B 的 `reserved=4` 枚举布局；没有沿用系统 A 的 `coast=4`、`insertion=5`。

## 派生函数说明

所有派生函数由 `rule_for` 的规则表驱动：`accepts` 仅对 `strict` 拒绝空标识，且分别检查时间戳和序号；`replayable` 判定回放域是否为 `none`；`flag_active` 对请求 bit 建掩码；其余函数直接使用阶段、分支和闭区间字段。

## 验证

- 在 `q02-protocol-adaptation` 运行 `./run_public_checks.sh`，退出码为 0。
- 额外运行本地只读核对：解析系统 B 全部 proto，仅收集绑定卡片，并逐字段对比源码；48 条规则和全部 528 个规则字段一致。
- 风险：公开检查仅抽样，但完整卡片逐字段核对覆盖了全部实现绑定结构；未修改头文件、测试、系统 A、系统 B 或映射文件。
