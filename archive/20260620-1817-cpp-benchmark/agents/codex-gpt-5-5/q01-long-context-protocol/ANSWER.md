# Q1 作答说明

## v6 真值

缩写：`strict/keep/auto` 分别为严格必填、保留空值、空则生成；`none/stage/vehicle/range` 分别为不回放、本级、本箭、本靶区；`T/O` 为时间戳/序号。

| 结构 | route | rev | id | replay | T/O | mode | phase | branch | band |
|---|---:|---:|---|---|---|---:|---:|---|---|
| stage_state | 576 | 2 | auto | vehicle | F/T | 0x01 | 2 | command | [191,219] |
| stage_states | 415 | 2 | strict | none | T/F | 0x0f | 1 | event | [85,201] |
| stage_state_view | 670 | 5 | keep | stage | T/F | 0x35 | 5 | command | [353,784] |
| stage_state_patch | 329 | 5 | keep | none | T/T | 0x2a | 1 | config | [489,1488] |
| stage_state_legacy | 482 | 9 | auto | range | T/T | 0x2c | 2 | event | [205,1189] |
| stage_state_v2 | 756 | 7 | auto | range | F/T | 0x19 | 2 | event | [75,622] |
| thrust_report | 385 | 2 | strict | range | F/F | 0x06 | 5 | command | [23,834] |
| thrust_reports | 482 | 2 | keep | stage | F/F | 0x2f | 1 | telemetry | [449,916] |
| thrust_report_view | 885 | 9 | strict | none | T/F | 0x1b | 2 | event | [71,559] |
| thrust_report_patch | 465 | 4 | keep | stage | T/T | 0x0f | 3 | command | [459,983] |
| thrust_report_legacy | 352 | 8 | auto | none | F/F | 0x08 | 6 | event | [350,545] |
| thrust_report_v2 | 986 | 7 | keep | vehicle | T/T | 0x04 | 1 | telemetry | [116,519] |
| nav_solution | 342 | 4 | strict | none | F/T | 0x3b | 6 | event | [290,612] |
| nav_solutions | 522 | 8 | strict | range | T/F | 0x0e | 6 | event | [408,794] |
| nav_solution_view | 892 | 5 | keep | stage | F/T | 0x36 | 1 | telemetry | [220,728] |
| nav_solution_patch | 798 | 3 | auto | stage | F/F | 0x27 | 0 | command | [98,1018] |
| nav_solution_legacy | 565 | 8 | keep | range | F/F | 0x10 | 6 | event | [414,872] |
| nav_solution_v2 | 591 | 2 | keep | vehicle | T/F | 0x04 | 0 | event | [351,716] |
| sep_event | 869 | 6 | strict | stage | F/F | 0x09 | 5 | config | [464,557] |
| sep_events | 277 | 8 | keep | range | T/F | 0x09 | 2 | event | [85,931] |
| sep_event_view | 981 | 9 | auto | vehicle | F/T | 0x20 | 0 | config | [341,1082] |
| sep_event_patch | 515 | 4 | auto | none | F/F | 0x3b | 0 | config | [135,814] |
| sep_event_legacy | 224 | 4 | keep | range | T/F | 0x3a | 0 | event | [136,432] |
| sep_event_v2 | 552 | 6 | strict | vehicle | F/F | 0x1f | 3 | telemetry | [42,259] |
| fts_status | 635 | 6 | strict | none | F/F | 0x04 | 0 | event | [308,432] |
| fts_statuses | 856 | 3 | strict | vehicle | T/F | 0x16 | 3 | telemetry | [328,549] |
| fts_status_view | 209 | 2 | keep | vehicle | F/T | 0x14 | 1 | telemetry | [445,721] |
| fts_status_patch | 825 | 9 | auto | none | F/F | 0x03 | 0 | event | [2,390] |
| fts_status_legacy | 122 | 6 | auto | stage | F/F | 0x26 | 1 | event | [209,233] |
| fts_status_v2 | 110 | 5 | auto | stage | F/F | 0x30 | 3 | event | [167,454] |
| payload_frame | 810 | 7 | keep | vehicle | T/F | 0x33 | 5 | telemetry | [142,766] |
| payload_frames | 425 | 4 | strict | range | F/F | 0x1f | 0 | telemetry | [277,1162] |
| payload_frame_view | 972 | 9 | keep | range | T/T | 0x2b | 3 | event | [481,1029] |
| payload_frame_patch | 667 | 7 | auto | range | F/T | 0x11 | 0 | command | [484,812] |
| payload_frame_legacy | 458 | 1 | strict | range | T/T | 0x08 | 1 | event | [333,700] |
| payload_frame_v2 | 803 | 6 | keep | stage | F/T | 0x1f | 1 | telemetry | [413,705] |
| link_quality | 412 | 9 | strict | range | T/F | 0x39 | 5 | telemetry | [158,248] |
| link_qualities | 761 | 9 | keep | range | F/F | 0x33 | 5 | command | [81,926] |
| link_quality_view | 517 | 9 | auto | none | T/T | 0x0c | 3 | command | [122,533] |
| link_quality_patch | 713 | 1 | keep | range | F/F | 0x26 | 5 | config | [390,865] |
| link_quality_legacy | 782 | 2 | keep | none | F/T | 0x37 | 5 | command | [185,592] |
| link_quality_v2 | 773 | 4 | strict | range | T/F | 0x39 | 1 | event | [374,931] |
| thermal_sample | 100 | 7 | keep | vehicle | T/T | 0x12 | 5 | event | [495,883] |
| thermal_samples | 978 | 6 | auto | stage | T/T | 0x10 | 2 | telemetry | [133,255] |
| thermal_sample_view | 520 | 9 | keep | none | F/T | 0x08 | 2 | event | [406,539] |
| thermal_sample_patch | 197 | 5 | strict | vehicle | T/T | 0x06 | 0 | command | [324,475] |
| thermal_sample_legacy | 130 | 9 | keep | range | F/T | 0x34 | 2 | config | [466,1209] |
| thermal_sample_v2 | 819 | 1 | strict | none | T/F | 0x20 | 5 | config | [286,669] |

每个表项先取 `v6.proto` 对应卡片；卡片显式指向变更日志时取最新 changelog 值；随后由 ERR-00 至 ERR-11 覆盖对应字段。实现中的规则表与此过程逐字段比对，48 条均一致。

## 文档优先级与陷阱处理

ADR-009 的优先级为：**勘误表 > changelog > v6.proto 注释 > v4 字段词典 > 历史代码**。

- 勘误覆盖示例：`stage_state.route_code` 采用 ERR-07 的 576；`stage_state.schema_rev` 和 `ordinal_required` 采用 ERR-09；`stage_state.replay_scope` 采用 ERR-11。
- 词典的废弃标记未用作真值。涉及重新启用或变更的字段，仍由 changelog/v6 卡片决定，例如 `stage_states.mode_flag` 采用 v4→v5 的 15。
- 单位漂移只按 v2→v3 中已经换算后的数值存入规则表，不再进行运行时乘法；例如 `thrust_reports.band` 为 `[449,916]`。
- 阶段编号使用 v5 重编号后的值，保留 4 号 `reserved` 空位；例如 `stage_state_view.phase_value` 为 5，`thrust_report_legacy.phase_value` 为 6。
- 未使用 `validator_v2.cpp`、`validator_v4.cpp` 或 `golden_v3.cpp` 作为冲突真值来源。
- 只按结构代号精确匹配规则：如 `sep_event`、`sep_events`、`sep_event_view` 分别独立读取，未按词形或族系继承字段。

## 派生函数

`accepts`、`replayable`、`flag_active`、`phase_rank`、`branch_of`、`within_band` 均从 `rule_for` 返回的同一规则表读取。模式位使用单独 bit mask 判断，没有把整个掩码作为单一布尔值。

## 验证

- 在 `q01-long-context-protocol` 运行 `./run_public_checks.sh`，退出码为 0。
- 额外运行本地只读核对：解析 v6 卡片、按 changelog 顺序覆盖并最后应用全部勘误表，再逐字段对比源码规则表；48 条规则的 528 个规则字段均一致。
- 风险：公开检查仅覆盖抽样；已用完整语料合成核对降低该风险，未修改公开头文件、测试或 corpus。
