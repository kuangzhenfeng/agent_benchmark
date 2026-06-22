# corpus 索引

本目录为 range.tc 协议演进语料。结构如下：

- `protocol/v1.proto` … `v6.proto`：六个版本的物模型；v6 为最新。
- `changelog/`：v1→v2 … v5→v6 变更日志。
- `errata/`：ERR-00 … ERR-11 勘误表（优先级最高）。
- `adr/ADR-009.md`：文档取舍优先级。
- `dict/field_dictionary_v4.md`：v4 字段词典（已滞后）。
- `legacy-code/`：v2、v4 历史实现（含已知缺陷）。
- `legacy-tests/golden_v3.cpp`：v3 历史测试（部分已失效）。

## 实现绑定结构清单（48 个）

| 族 | 变体 |
|----|------|
| stage | stage_state, stage_states, stage_state_view, stage_state_patch, stage_state_legacy, stage_state_v2 |
| propulsion | thrust_report, thrust_reports, thrust_report_view, thrust_report_patch, thrust_report_legacy, thrust_report_v2 |
| guidance | nav_solution, nav_solutions, nav_solution_view, nav_solution_patch, nav_solution_legacy, nav_solution_v2 |
| separation | sep_event, sep_events, sep_event_view, sep_event_patch, sep_event_legacy, sep_event_v2 |
| rangesafety | fts_status, fts_statuses, fts_status_view, fts_status_patch, fts_status_legacy, fts_status_v2 |
| payload | payload_frame, payload_frames, payload_frame_view, payload_frame_patch, payload_frame_legacy, payload_frame_v2 |
| groundlink | link_quality, link_qualities, link_quality_view, link_quality_patch, link_quality_legacy, link_quality_v2 |
| thermal | thermal_sample, thermal_samples, thermal_sample_view, thermal_sample_patch, thermal_sample_legacy, thermal_sample_v2 |

> 提示：v6 正确值散落在上述多份文档中。务必阅读 ADR-009 的优先级规则。
