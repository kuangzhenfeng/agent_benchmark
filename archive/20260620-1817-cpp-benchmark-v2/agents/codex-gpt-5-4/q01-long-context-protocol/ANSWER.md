# Q1 作答说明

## v6 真值来源

按 `ADR-009` 执行取舍：`勘误表 > changelog > v6.proto 注释 > 字段词典(v4) > 历史代码`。

- `field_dictionary_v4.md`、`validator_v2.cpp`、`validator_v4.cpp` 只作为低优先级交叉核对，不覆盖高优先级来源。
- 下表列出 48 个实现绑定结构的最终真值；`来源摘要` 按字段标明最终落点文档。

| 结构 | route | rev | id | replay | stamp | ordinal | mode | phase | branch | band | 来源摘要 |
| --- | ---: | ---: | --- | --- | --- | --- | ---: | ---: | --- | --- | --- |
| stage_state | 576 | 2 | auto_gen | vehicle | false | true | 1 | 2 | command | [191, 219] | route_code=ERR-07, schema_rev=ERR-09, id_policy=v5-to-v6, replay_scope=ERR-11, stamp_required=v6.proto, ordinal_required=ERR-09, mode_flag=v6.proto, phase_value=v4-to-v5, branch=v5-to-v6, band=v6.proto |
| stage_states | 415 | 2 | strict | none | true | false | 15 | 1 | event | [85, 201] | route_code=v6.proto, schema_rev=v6.proto, id_policy=ERR-08, replay_scope=v4-to-v5, stamp_required=v6.proto, ordinal_required=v6.proto, mode_flag=v4-to-v5, phase_value=v6.proto, branch=v6.proto, band=v5-to-v6 |
| stage_state_view | 670 | 5 | keep_blank | stage | true | false | 53 | 5 | command | [353, 784] | route_code=v4-to-v5, schema_rev=v6.proto, id_policy=v4-to-v5, replay_scope=v6.proto, stamp_required=v6.proto, ordinal_required=v6.proto, mode_flag=v6.proto, phase_value=v4-to-v5, branch=ERR-05, band=v2-to-v3 |
| stage_state_patch | 329 | 5 | keep_blank | none | true | true | 42 | 1 | config | [489, 1488] | route_code=v6.proto, schema_rev=v4-to-v5, id_policy=ERR-00, replay_scope=v6.proto, stamp_required=v6.proto, ordinal_required=ERR-09, mode_flag=v4-to-v5, phase_value=v6.proto, branch=v6.proto, band=v6.proto |
| stage_state_legacy | 482 | 9 | auto_gen | range | true | true | 44 | 2 | event | [205, 1189] | route_code=v5-to-v6, schema_rev=v4-to-v5, id_policy=v4-to-v5, replay_scope=ERR-01, stamp_required=v6.proto, ordinal_required=ERR-09, mode_flag=v6.proto, phase_value=v4-to-v5, branch=v6.proto, band=v6.proto |
| stage_state_v2 | 756 | 7 | auto_gen | range | false | true | 25 | 2 | event | [75, 622] | route_code=v6.proto, schema_rev=ERR-09, id_policy=v4-to-v5, replay_scope=ERR-09, stamp_required=ERR-11, ordinal_required=v6.proto, mode_flag=v6.proto, phase_value=v4-to-v5, branch=v4-to-v5, band=v2-to-v3 |
| thrust_report | 385 | 2 | strict | range | false | false | 6 | 5 | command | [23, 834] | route_code=ERR-02, schema_rev=v5-to-v6, id_policy=v6.proto, replay_scope=v4-to-v5, stamp_required=ERR-07, ordinal_required=ERR-04, mode_flag=v6.proto, phase_value=v4-to-v5, branch=v6.proto, band=v6.proto |
| thrust_reports | 482 | 2 | keep_blank | stage | false | false | 47 | 1 | telemetry | [449, 916] | route_code=v6.proto, schema_rev=ERR-07, id_policy=ERR-09, replay_scope=ERR-09, stamp_required=v6.proto, ordinal_required=v6.proto, mode_flag=v6.proto, phase_value=v6.proto, branch=v5-to-v6, band=v2-to-v3 |
| thrust_report_view | 885 | 9 | strict | none | true | false | 27 | 2 | event | [71, 559] | route_code=v6.proto, schema_rev=v4-to-v5, id_policy=v4-to-v5, replay_scope=v5-to-v6, stamp_required=v6.proto, ordinal_required=v6.proto, mode_flag=v6.proto, phase_value=v6.proto, branch=v4-to-v5, band=ERR-08 |
| thrust_report_patch | 465 | 4 | keep_blank | stage | true | true | 15 | 3 | command | [459, 983] | route_code=v6.proto, schema_rev=v6.proto, id_policy=v4-to-v5, replay_scope=v6.proto, stamp_required=ERR-07, ordinal_required=ERR-06, mode_flag=v6.proto, phase_value=v4-to-v5, branch=v4-to-v5, band=v6.proto |
| thrust_report_legacy | 352 | 8 | auto_gen | none | false | false | 8 | 6 | event | [350, 545] | route_code=v6.proto, schema_rev=v6.proto, id_policy=v6.proto, replay_scope=v6.proto, stamp_required=ERR-03, ordinal_required=v6.proto, mode_flag=v6.proto, phase_value=v4-to-v5, branch=v6.proto, band=v6.proto |
| thrust_report_v2 | 986 | 7 | keep_blank | vehicle | true | true | 4 | 1 | telemetry | [116, 519] | route_code=ERR-03, schema_rev=ERR-11, id_policy=v6.proto, replay_scope=ERR-03, stamp_required=v6.proto, ordinal_required=ERR-01, mode_flag=v6.proto, phase_value=v6.proto, branch=v4-to-v5, band=v6.proto |
| nav_solution | 342 | 4 | strict | none | false | true | 59 | 6 | event | [290, 612] | route_code=v6.proto, schema_rev=ERR-01, id_policy=v4-to-v5, replay_scope=ERR-03, stamp_required=v6.proto, ordinal_required=ERR-05, mode_flag=v4-to-v5, phase_value=v5-to-v6, branch=v6.proto, band=v5-to-v6 |
| nav_solutions | 522 | 8 | strict | range | true | false | 14 | 6 | event | [408, 794] | route_code=v6.proto, schema_rev=ERR-01, id_policy=ERR-01, replay_scope=v4-to-v5, stamp_required=v6.proto, ordinal_required=ERR-08, mode_flag=v4-to-v5, phase_value=v4-to-v5, branch=v4-to-v5, band=v5-to-v6 |
| nav_solution_view | 892 | 5 | keep_blank | stage | false | true | 54 | 1 | telemetry | [220, 728] | route_code=v6.proto, schema_rev=v6.proto, id_policy=v4-to-v5, replay_scope=ERR-08, stamp_required=ERR-07, ordinal_required=ERR-09, mode_flag=v4-to-v5, phase_value=v5-to-v6, branch=v4-to-v5, band=ERR-03 |
| nav_solution_patch | 798 | 3 | auto_gen | stage | false | false | 39 | 0 | command | [98, 1018] | route_code=v4-to-v5, schema_rev=v5-to-v6, id_policy=v6.proto, replay_scope=v4-to-v5, stamp_required=ERR-06, ordinal_required=ERR-01, mode_flag=v6.proto, phase_value=v6.proto, branch=v5-to-v6, band=v4-to-v5 |
| nav_solution_legacy | 565 | 8 | keep_blank | range | false | false | 16 | 6 | event | [414, 872] | route_code=v5-to-v6, schema_rev=v4-to-v5, id_policy=v4-to-v5, replay_scope=v4-to-v5, stamp_required=v6.proto, ordinal_required=ERR-05, mode_flag=v6.proto, phase_value=v4-to-v5, branch=v4-to-v5, band=v6.proto |
| nav_solution_v2 | 591 | 2 | keep_blank | vehicle | true | false | 4 | 0 | event | [351, 716] | route_code=v6.proto, schema_rev=ERR-09, id_policy=ERR-10, replay_scope=v6.proto, stamp_required=v6.proto, ordinal_required=ERR-11, mode_flag=v4-to-v5, phase_value=v6.proto, branch=v4-to-v5, band=ERR-04 |
| sep_event | 869 | 6 | strict | stage | false | false | 9 | 5 | config | [464, 557] | route_code=v4-to-v5, schema_rev=v6.proto, id_policy=v4-to-v5, replay_scope=v4-to-v5, stamp_required=v6.proto, ordinal_required=v6.proto, mode_flag=v4-to-v5, phase_value=v6.proto, branch=ERR-10, band=ERR-04 |
| sep_events | 277 | 8 | keep_blank | range | true | false | 9 | 2 | event | [85, 931] | route_code=v6.proto, schema_rev=ERR-02, id_policy=ERR-01, replay_scope=ERR-09, stamp_required=v6.proto, ordinal_required=ERR-04, mode_flag=v4-to-v5, phase_value=v4-to-v5, branch=ERR-05, band=v6.proto |
| sep_event_view | 981 | 9 | auto_gen | vehicle | false | true | 32 | 0 | config | [341, 1082] | route_code=v4-to-v5, schema_rev=ERR-11, id_policy=v4-to-v5, replay_scope=v4-to-v5, stamp_required=ERR-11, ordinal_required=v6.proto, mode_flag=v6.proto, phase_value=v4-to-v5, branch=v4-to-v5, band=ERR-04 |
| sep_event_patch | 515 | 4 | auto_gen | none | false | false | 59 | 0 | config | [135, 814] | route_code=v6.proto, schema_rev=v5-to-v6, id_policy=v6.proto, replay_scope=v4-to-v5, stamp_required=v6.proto, ordinal_required=ERR-10, mode_flag=v6.proto, phase_value=v6.proto, branch=ERR-04, band=v6.proto |
| sep_event_legacy | 224 | 4 | keep_blank | range | true | false | 58 | 0 | event | [136, 432] | route_code=v6.proto, schema_rev=ERR-11, id_policy=v5-to-v6, replay_scope=ERR-09, stamp_required=v6.proto, ordinal_required=ERR-10, mode_flag=v6.proto, phase_value=v6.proto, branch=v4-to-v5, band=v5-to-v6 |
| sep_event_v2 | 552 | 6 | strict | vehicle | false | false | 31 | 3 | telemetry | [42, 259] | route_code=v6.proto, schema_rev=ERR-11, id_policy=v4-to-v5, replay_scope=ERR-01, stamp_required=v6.proto, ordinal_required=v6.proto, mode_flag=v6.proto, phase_value=v6.proto, branch=ERR-03, band=v6.proto |
| fts_status | 635 | 6 | strict | none | false | false | 4 | 0 | event | [308, 432] | route_code=v6.proto, schema_rev=v5-to-v6, id_policy=v4-to-v5, replay_scope=v4-to-v5, stamp_required=ERR-08, ordinal_required=v6.proto, mode_flag=v4-to-v5, phase_value=v6.proto, branch=v4-to-v5, band=ERR-02 |
| fts_statuses | 856 | 3 | strict | vehicle | true | false | 22 | 3 | telemetry | [328, 549] | route_code=v6.proto, schema_rev=ERR-06, id_policy=v4-to-v5, replay_scope=ERR-09, stamp_required=v6.proto, ordinal_required=ERR-11, mode_flag=v6.proto, phase_value=v4-to-v5, branch=v6.proto, band=v6.proto |
| fts_status_view | 209 | 2 | keep_blank | vehicle | false | true | 20 | 1 | telemetry | [445, 721] | route_code=v6.proto, schema_rev=v6.proto, id_policy=v4-to-v5, replay_scope=v6.proto, stamp_required=ERR-07, ordinal_required=v6.proto, mode_flag=v6.proto, phase_value=v6.proto, branch=ERR-03, band=v2-to-v3 |
| fts_status_patch | 825 | 9 | auto_gen | none | false | false | 3 | 0 | event | [2, 390] | route_code=v6.proto, schema_rev=v6.proto, id_policy=ERR-05, replay_scope=ERR-07, stamp_required=ERR-10, ordinal_required=v6.proto, mode_flag=v6.proto, phase_value=v4-to-v5, branch=v4-to-v5, band=v2-to-v3 |
| fts_status_legacy | 122 | 6 | auto_gen | stage | false | false | 38 | 1 | event | [209, 233] | route_code=v6.proto, schema_rev=v5-to-v6, id_policy=v4-to-v5, replay_scope=v6.proto, stamp_required=v6.proto, ordinal_required=v6.proto, mode_flag=v6.proto, phase_value=v6.proto, branch=v4-to-v5, band=v2-to-v3 |
| fts_status_v2 | 110 | 5 | auto_gen | stage | false | false | 48 | 3 | event | [167, 454] | route_code=v5-to-v6, schema_rev=v6.proto, id_policy=v6.proto, replay_scope=ERR-09, stamp_required=v6.proto, ordinal_required=v6.proto, mode_flag=v6.proto, phase_value=v6.proto, branch=v4-to-v5, band=v6.proto |
| payload_frame | 810 | 7 | keep_blank | vehicle | true | false | 51 | 5 | telemetry | [142, 766] | route_code=v6.proto, schema_rev=v6.proto, id_policy=v6.proto, replay_scope=v6.proto, stamp_required=v6.proto, ordinal_required=v6.proto, mode_flag=v4-to-v5, phase_value=v6.proto, branch=v6.proto, band=v6.proto |
| payload_frames | 425 | 4 | strict | range | false | false | 31 | 0 | telemetry | [277, 1162] | route_code=v5-to-v6, schema_rev=v4-to-v5, id_policy=v4-to-v5, replay_scope=ERR-09, stamp_required=v6.proto, ordinal_required=v6.proto, mode_flag=v4-to-v5, phase_value=v6.proto, branch=v6.proto, band=v2-to-v3 |
| payload_frame_view | 972 | 9 | keep_blank | range | true | true | 43 | 3 | event | [481, 1029] | route_code=ERR-04, schema_rev=v4-to-v5, id_policy=ERR-02, replay_scope=v5-to-v6, stamp_required=v6.proto, ordinal_required=ERR-02, mode_flag=v6.proto, phase_value=v6.proto, branch=v6.proto, band=v6.proto |
| payload_frame_patch | 667 | 7 | auto_gen | range | false | true | 17 | 0 | command | [484, 812] | route_code=ERR-06, schema_rev=v4-to-v5, id_policy=v6.proto, replay_scope=ERR-01, stamp_required=ERR-10, ordinal_required=v6.proto, mode_flag=v4-to-v5, phase_value=v6.proto, branch=ERR-02, band=ERR-11 |
| payload_frame_legacy | 458 | 1 | strict | range | true | true | 8 | 1 | event | [333, 700] | route_code=v5-to-v6, schema_rev=ERR-11, id_policy=ERR-07, replay_scope=v4-to-v5, stamp_required=ERR-02, ordinal_required=v6.proto, mode_flag=v6.proto, phase_value=v6.proto, branch=ERR-10, band=v6.proto |
| payload_frame_v2 | 803 | 6 | keep_blank | stage | false | true | 31 | 1 | telemetry | [413, 705] | route_code=v4-to-v5, schema_rev=ERR-08, id_policy=v6.proto, replay_scope=ERR-01, stamp_required=v6.proto, ordinal_required=ERR-01, mode_flag=v6.proto, phase_value=v6.proto, branch=v5-to-v6, band=ERR-09 |
| link_quality | 412 | 9 | strict | range | true | false | 57 | 5 | telemetry | [158, 248] | route_code=ERR-06, schema_rev=v6.proto, id_policy=v5-to-v6, replay_scope=v4-to-v5, stamp_required=v6.proto, ordinal_required=ERR-00, mode_flag=v6.proto, phase_value=v6.proto, branch=ERR-03, band=v6.proto |
| link_qualities | 761 | 9 | keep_blank | range | false | false | 51 | 5 | command | [81, 926] | route_code=v6.proto, schema_rev=v4-to-v5, id_policy=ERR-10, replay_scope=v4-to-v5, stamp_required=v6.proto, ordinal_required=v6.proto, mode_flag=v6.proto, phase_value=v6.proto, branch=ERR-06, band=v4-to-v5 |
| link_quality_view | 517 | 9 | auto_gen | none | true | true | 12 | 3 | command | [122, 533] | route_code=v6.proto, schema_rev=v5-to-v6, id_policy=ERR-03, replay_scope=ERR-10, stamp_required=v6.proto, ordinal_required=v6.proto, mode_flag=v4-to-v5, phase_value=v6.proto, branch=v4-to-v5, band=v6.proto |
| link_quality_patch | 713 | 1 | keep_blank | range | false | false | 38 | 5 | config | [390, 865] | route_code=v5-to-v6, schema_rev=v4-to-v5, id_policy=ERR-09, replay_scope=v6.proto, stamp_required=ERR-08, ordinal_required=ERR-09, mode_flag=v6.proto, phase_value=v5-to-v6, branch=v6.proto, band=v6.proto |
| link_quality_legacy | 782 | 2 | keep_blank | none | false | true | 55 | 5 | command | [185, 592] | route_code=v6.proto, schema_rev=v5-to-v6, id_policy=v5-to-v6, replay_scope=v5-to-v6, stamp_required=v6.proto, ordinal_required=ERR-10, mode_flag=v6.proto, phase_value=v4-to-v5, branch=v4-to-v5, band=v6.proto |
| link_quality_v2 | 773 | 4 | strict | range | true | false | 57 | 1 | event | [374, 931] | route_code=v6.proto, schema_rev=ERR-10, id_policy=ERR-03, replay_scope=v5-to-v6, stamp_required=ERR-10, ordinal_required=v6.proto, mode_flag=v6.proto, phase_value=v6.proto, branch=v6.proto, band=ERR-04 |
| thermal_sample | 100 | 7 | keep_blank | vehicle | true | true | 18 | 5 | event | [495, 883] | route_code=v6.proto, schema_rev=ERR-10, id_policy=v4-to-v5, replay_scope=v6.proto, stamp_required=v6.proto, ordinal_required=ERR-04, mode_flag=v4-to-v5, phase_value=v4-to-v5, branch=ERR-09, band=v6.proto |
| thermal_samples | 978 | 6 | auto_gen | stage | true | true | 16 | 2 | telemetry | [133, 255] | route_code=ERR-09, schema_rev=v6.proto, id_policy=v6.proto, replay_scope=ERR-06, stamp_required=ERR-06, ordinal_required=v6.proto, mode_flag=v4-to-v5, phase_value=v6.proto, branch=v4-to-v5, band=v6.proto |
| thermal_sample_view | 520 | 9 | keep_blank | none | false | true | 8 | 2 | event | [406, 539] | route_code=v6.proto, schema_rev=ERR-08, id_policy=ERR-08, replay_scope=ERR-02, stamp_required=ERR-09, ordinal_required=ERR-02, mode_flag=v6.proto, phase_value=v6.proto, branch=v5-to-v6, band=v6.proto |
| thermal_sample_patch | 197 | 5 | strict | vehicle | true | true | 6 | 0 | command | [324, 475] | route_code=v6.proto, schema_rev=v5-to-v6, id_policy=v5-to-v6, replay_scope=v4-to-v5, stamp_required=ERR-07, ordinal_required=ERR-02, mode_flag=v6.proto, phase_value=v4-to-v5, branch=v4-to-v5, band=v5-to-v6 |
| thermal_sample_legacy | 130 | 9 | keep_blank | range | false | true | 52 | 2 | config | [466, 1209] | route_code=v5-to-v6, schema_rev=ERR-09, id_policy=v4-to-v5, replay_scope=v6.proto, stamp_required=v6.proto, ordinal_required=v6.proto, mode_flag=v6.proto, phase_value=v6.proto, branch=v4-to-v5, band=v6.proto |
| thermal_sample_v2 | 819 | 1 | strict | none | true | false | 32 | 5 | config | [286, 669] | route_code=v4-to-v5, schema_rev=v5-to-v6, id_policy=v6.proto, replay_scope=v6.proto, stamp_required=v6.proto, ordinal_required=v6.proto, mode_flag=v6.proto, phase_value=v4-to-v5, branch=ERR-05, band=v6.proto |

## 陷阱处理

- 被勘误表推翻的 proto/changelog：
  - `stage_state_patch.id_policy` 最终按 `ERR-00` 取 `keep_blank`，没有沿用 `v6.proto` 的 `strict`。
  - `payload_frame_patch.band` 最终按 `ERR-11` 取 `[484, 812]`，没有沿用 `v6.proto` 的 `[316, 463]`。
  - `sep_event.branch` 最终按 `ERR-10` 取 `config`，没有沿用更低优先级来源。
- 字段词典的“已废弃/统一”不当真值：
  - `nav_solution.id_policy` 在 `field_dictionary_v4.md` 仍是统一前标注，但最终按 `v4-to-v5` 取 `strict`。
  - `fts_status_legacy.id_policy` 不沿用词典旧值，最终按 `v4-to-v5` 取 `auto_gen`。
- 单位漂移（ms → 0.1ms）：
  - `stage_state_view.band`、`thrust_reports.band`、`fts_status_view.band` 都追到 `v2-to-v3.md`，按 v3 后口径使用对应区间。
- 枚举重编号：
  - 遇到 `v5 枚举重编号` 注释时，只采用“重编号后”的值，例如 `payload_frame.mode_flag=51`、`nav_solution_v2.mode_flag=4`、`thermal_sample.phase_value=5`。
- 历史代码/历史测试过时：
  - `thrust_report.route_code` 若抄 `v6.proto` 会错，最终需按 `ERR-02` 取 `385`。
  - `sep_event_legacy.schema_rev` 最终按 `ERR-11` 为 `4`，不沿用旧实现时代的直觉值。
- 近似变体名称混用：
  - `link_quality`、`link_qualities`、`link_quality_view` 分别独立取值。
  - `payload_frame`、`payload_frame_patch`、`payload_frame_v2` 的 `replay_scope` / `band` / `ordinal_required` 都分别读取，没有按族共享默认值。

## 文档优先级

`ADR-009` 的优先级是：

`勘误表 > changelog > v6.proto 注释 > 字段词典(v4) > 历史代码`

我的处理方式：

- 先以 `v6.proto` 的 48 个实现绑定结构为索引收集基础值。
- 再按版本顺序应用 `changelog`，只接受“变更后的最终值”。
- 最后把 `ERR-00` 到 `ERR-11` 全部覆写到最终规则表。
- `field_dictionary_v4.md` 与 `validator_v2.cpp` / `validator_v4.cpp` 只做低优先级交叉核对，不参与覆盖。

## 验证

- 公开检查：在 `q01-long-context-protocol/` 下执行 `./run_public_checks.sh`，通过。
- 额外自测：写了本地脚本从 `v6.proto + changelog + errata` 机械抽取 48 条真值，再与 `src/range_validator.cpp` 全量对表，`48 * 10` 个字段点位比对结果为 `0 mismatch`。
- 仍未覆盖的风险：隐藏评测若检查我对文档语义的理解而非字段表值，风险主要集中在语料表述歧义；当前规则表与已抽取文档真值是一致的。
