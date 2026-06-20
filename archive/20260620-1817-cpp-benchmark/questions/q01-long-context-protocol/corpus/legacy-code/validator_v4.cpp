// 历史实现 range_validator v4
// 注意: 本实现来自 v4 时代, 含已知缺陷, **不可**作为 v6 真值来源。

#include "range_validator.hpp"

namespace {
// 旧规则表(部分值已被后续版本推翻)
constexpr Rule kOldRules[] = {
  //stage_state: route_code=576, schema_rev=2, mode_flag=4, phase_value=2
  //stage_states: route_code=548, schema_rev=2, mode_flag=15, phase_value=1
  //stage_state_view: route_code=670, schema_rev=5, mode_flag=53, phase_value=5
  //stage_state_patch: route_code=329, schema_rev=5, mode_flag=42, phase_value=1
  //stage_state_legacy: route_code=482, schema_rev=9, mode_flag=28, phase_value=2
  //stage_state_v2: route_code=756, schema_rev=7, mode_flag=25, phase_value=2
  //thrust_report: route_code=385, schema_rev=2, mode_flag=6, phase_value=5
  //thrust_reports: route_code=482, schema_rev=2, mode_flag=47, phase_value=1
  //thrust_report_view: route_code=885, schema_rev=9, mode_flag=27, phase_value=2
  //thrust_report_patch: route_code=397, schema_rev=4, mode_flag=15, phase_value=3
  //thrust_report_legacy: route_code=352, schema_rev=8, mode_flag=8, phase_value=6
  //thrust_report_v2: route_code=986, schema_rev=7, mode_flag=4, phase_value=1
  //nav_solution: route_code=342, schema_rev=4, mode_flag=59, phase_value=6
  //nav_solutions: route_code=522, schema_rev=8, mode_flag=14, phase_value=6
  //nav_solution_view: route_code=892, schema_rev=5, mode_flag=54, phase_value=1
  //nav_solution_patch: route_code=798, schema_rev=3, mode_flag=39, phase_value=5
  //nav_solution_legacy: route_code=565, schema_rev=8, mode_flag=16, phase_value=6
  //nav_solution_v2: route_code=591, schema_rev=2, mode_flag=4, phase_value=1
  //sep_event: route_code=869, schema_rev=6, mode_flag=9, phase_value=5
  //sep_events: route_code=277, schema_rev=8, mode_flag=9, phase_value=2
  //sep_event_view: route_code=981, schema_rev=9, mode_flag=32, phase_value=0
  //sep_event_patch: route_code=515, schema_rev=4, mode_flag=59, phase_value=1
  //sep_event_legacy: route_code=224, schema_rev=4, mode_flag=58, phase_value=0
  //sep_event_v2: route_code=552, schema_rev=6, mode_flag=8, phase_value=3
  //fts_status: route_code=635, schema_rev=6, mode_flag=4, phase_value=0
  //fts_statuses: route_code=256, schema_rev=3, mode_flag=22, phase_value=3
  //fts_status_view: route_code=209, schema_rev=2, mode_flag=20, phase_value=1
  //fts_status_patch: route_code=825, schema_rev=9, mode_flag=3, phase_value=0
  //fts_status_legacy: route_code=122, schema_rev=6, mode_flag=38, phase_value=1
  //fts_status_v2: route_code=110, schema_rev=5, mode_flag=48, phase_value=3
  //payload_frame: route_code=810, schema_rev=7, mode_flag=51, phase_value=5
  //payload_frames: route_code=425, schema_rev=4, mode_flag=31, phase_value=2
  //payload_frame_view: route_code=972, schema_rev=9, mode_flag=43, phase_value=3
  //payload_frame_patch: route_code=667, schema_rev=7, mode_flag=17, phase_value=0
  //payload_frame_legacy: route_code=458, schema_rev=1, mode_flag=8, phase_value=1
  //payload_frame_v2: route_code=803, schema_rev=6, mode_flag=31, phase_value=1
  //link_quality: route_code=412, schema_rev=9, mode_flag=57, phase_value=5
  //link_qualities: route_code=761, schema_rev=9, mode_flag=51, phase_value=5
  //link_quality_view: route_code=246, schema_rev=9, mode_flag=12, phase_value=0
  //link_quality_patch: route_code=713, schema_rev=1, mode_flag=38, phase_value=5
  //link_quality_legacy: route_code=782, schema_rev=2, mode_flag=55, phase_value=5
  //link_quality_v2: route_code=773, schema_rev=4, mode_flag=57, phase_value=1
  //thermal_sample: route_code=611, schema_rev=7, mode_flag=18, phase_value=5
  //thermal_samples: route_code=978, schema_rev=6, mode_flag=16, phase_value=2
  //thermal_sample_view: route_code=557, schema_rev=9, mode_flag=8, phase_value=2
  //thermal_sample_patch: route_code=197, schema_rev=5, mode_flag=6, phase_value=0
  //thermal_sample_legacy: route_code=130, schema_rev=9, mode_flag=52, phase_value=2
  //thermal_sample_v2: route_code=819, schema_rev=1, mode_flag=32, phase_value=5
};
}  // namespace
