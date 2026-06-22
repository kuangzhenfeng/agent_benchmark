# Q2 作答说明

## 适配摘要

下表是系统 B 中 48 个“是否实现绑定: 是”结构的最终真值。我只把这些结构写入了 `src/protocol_adapter.cpp`。

| 结构 | route | rev | id | replay | stamp | ordinal | mode | phase | branch | band |
| --- | ---: | ---: | --- | --- | --- | --- | ---: | ---: | --- | --- |
| stage_state | 765 | 5 | keep_blank | vehicle | true | true | 11 | 3 | config | [313, 925] |
| stage_states | 924 | 7 | keep_blank | none | true | true | 29 | 0 | event | [295, 1082] |
| stage_state_view | 960 | 7 | keep_blank | range | false | true | 62 | 2 | command | [348, 777] |
| stage_state_patch | 694 | 5 | auto_gen | vehicle | false | true | 2 | 0 | event | [54, 739] |
| stage_state_legacy | 105 | 7 | keep_blank | vehicle | true | false | 33 | 1 | event | [389, 498] |
| stage_state_v2 | 421 | 5 | keep_blank | vehicle | true | false | 23 | 3 | telemetry | [0, 112] |
| thrust_report | 709 | 8 | keep_blank | stage | true | true | 1 | 1 | event | [362, 486] |
| thrust_reports | 521 | 2 | strict | none | true | false | 48 | 0 | event | [202, 452] |
| thrust_report_view | 389 | 8 | strict | vehicle | true | false | 33 | 5 | event | [30, 435] |
| thrust_report_patch | 216 | 7 | keep_blank | vehicle | false | false | 16 | 0 | config | [333, 1120] |
| thrust_report_legacy | 242 | 9 | auto_gen | vehicle | true | false | 14 | 6 | config | [196, 864] |
| thrust_report_v2 | 453 | 2 | keep_blank | vehicle | true | false | 51 | 5 | config | [89, 811] |
| nav_solution | 700 | 2 | auto_gen | none | true | false | 16 | 1 | command | [115, 1083] |
| nav_solutions | 583 | 5 | keep_blank | none | true | true | 10 | 0 | command | [158, 406] |
| nav_solution_view | 147 | 8 | keep_blank | none | false | false | 6 | 1 | command | [67, 738] |
| nav_solution_patch | 838 | 2 | keep_blank | none | false | true | 3 | 5 | event | [419, 805] |
| nav_solution_legacy | 617 | 2 | keep_blank | none | true | true | 54 | 1 | command | [241, 796] |
| nav_solution_v2 | 701 | 8 | keep_blank | none | true | true | 11 | 0 | command | [237, 785] |
| sep_event | 786 | 5 | auto_gen | range | false | true | 26 | 6 | command | [321, 610] |
| sep_events | 625 | 8 | auto_gen | range | true | false | 59 | 1 | command | [194, 1044] |
| sep_event_view | 867 | 6 | auto_gen | none | true | false | 61 | 2 | command | [253, 1129] |
| sep_event_patch | 601 | 5 | keep_blank | none | true | true | 30 | 2 | config | [31, 311] |
| sep_event_legacy | 427 | 4 | keep_blank | vehicle | false | false | 35 | 3 | telemetry | [381, 1049] |
| sep_event_v2 | 644 | 6 | strict | stage | true | false | 43 | 1 | event | [482, 1430] |
| fts_status | 103 | 7 | strict | none | false | true | 26 | 0 | config | [25, 714] |
| fts_statuses | 289 | 3 | auto_gen | none | true | false | 53 | 5 | event | [17, 885] |
| fts_status_view | 915 | 4 | auto_gen | stage | false | true | 34 | 2 | telemetry | [223, 497] |
| fts_status_patch | 809 | 9 | keep_blank | none | false | true | 7 | 5 | command | [389, 955] |
| fts_status_legacy | 476 | 5 | auto_gen | vehicle | true | true | 15 | 2 | config | [103, 1073] |
| fts_status_v2 | 429 | 5 | auto_gen | vehicle | true | true | 34 | 1 | event | [19, 159] |
| payload_frame | 108 | 5 | auto_gen | vehicle | false | true | 45 | 5 | telemetry | [363, 994] |
| payload_frames | 593 | 7 | auto_gen | range | true | true | 4 | 1 | config | [419, 832] |
| payload_frame_view | 175 | 9 | strict | vehicle | true | true | 3 | 0 | telemetry | [99, 880] |
| payload_frame_patch | 697 | 5 | keep_blank | none | true | true | 33 | 3 | command | [153, 173] |
| payload_frame_legacy | 792 | 5 | keep_blank | stage | true | false | 3 | 1 | telemetry | [202, 630] |
| payload_frame_v2 | 255 | 1 | strict | range | false | true | 31 | 0 | event | [36, 463] |
| link_quality | 239 | 7 | strict | range | true | true | 40 | 1 | config | [125, 249] |
| link_qualities | 683 | 6 | keep_blank | vehicle | false | false | 1 | 2 | config | [471, 791] |
| link_quality_view | 509 | 9 | strict | range | false | true | 3 | 0 | command | [136, 428] |
| link_quality_patch | 386 | 8 | auto_gen | none | true | true | 46 | 2 | config | [220, 302] |
| link_quality_legacy | 424 | 8 | keep_blank | vehicle | true | true | 61 | 5 | telemetry | [277, 1022] |
| link_quality_v2 | 923 | 1 | strict | stage | true | true | 59 | 0 | config | [303, 1110] |
| thermal_sample | 454 | 2 | strict | range | false | true | 23 | 1 | command | [302, 608] |
| thermal_samples | 127 | 5 | keep_blank | vehicle | true | false | 60 | 5 | telemetry | [214, 613] |
| thermal_sample_view | 716 | 3 | auto_gen | range | true | false | 59 | 0 | config | [330, 629] |
| thermal_sample_patch | 821 | 7 | auto_gen | none | true | true | 2 | 3 | telemetry | [447, 1033] |
| thermal_sample_legacy | 749 | 4 | strict | none | false | false | 26 | 3 | event | [494, 908] |
| thermal_sample_v2 | 316 | 3 | strict | stage | false | true | 2 | 2 | event | [121, 234] |

## 防止误映射的做法

- 先用 `系统A/变更映射.json` 里的 `C++结构代号 -> orbit.tc 结构代号` 只做定位，不搬运系统 A 的任何字段值。
- 进入系统 B 后，只接受同时满足“`结构代号` 精确匹配”和“`是否实现绑定: 是`”的卡片。
- 对单/复数、`view` / `patch` / `legacy` / `v2` 近似名称全部逐条读取，避免按族或位置做默认继承。
- `phase_value` 直接读系统 B 注释，不复用系统 A 编号：系统 B 明确是 `coast=5`、`insertion=6`，并且 `reserved=4` 是占位。
- 人工抽样时重点看了最容易误抄的结构：`stage_state`、`payload_frame`、`sep_event_v2`、`thermal_sample_v2`。

## 派生函数说明

- `accepts`：只有 `IdPolicy::strict` 才拒绝空 `identifier`；`stamp_required` 与 `ordinal_required` 独立判断。
- `replayable`：等价于 `replay_scope != ReplayScope::none`。
- `flag_active`：严格按 `bit0..bit5` 逐位取掩码，不把整个 `mode_flag` 当类别。
- `phase_rank`：直接返回规则表中的系统 B 阶段编号。
- `branch_of`：直接返回规则表中的 `branch`。
- `within_band`：直接判断样本是否落在 `[band_min, band_max]` 内。

## 验证

- 公开检查：在 `q02-protocol-adaptation/` 下执行 `./run_public_checks.sh`，通过。
- 额外自测：写了本地脚本从系统 B 四个 proto 的注释机械抽取 48 条真值，再与 `src/protocol_adapter.cpp` 全量对表，结果 `0 mismatch`。
- 仍未覆盖的风险：隐藏评测若检查范围外结构是否被误加规则项，目前实现只保留 48 个绑定结构，不会把上下文结构带入规则表。
