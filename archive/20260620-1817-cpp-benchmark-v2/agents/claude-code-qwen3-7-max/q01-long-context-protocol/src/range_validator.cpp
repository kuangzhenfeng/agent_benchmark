#include "range_validator.hpp"

#include <array>

namespace {

// 规则表: 按 ADR-009 优先级(勘误表 > changelog > v6.proto > 词典 > 历史代码)交叉核对后的 v6 真值
constexpr std::array<FieldRule, 48> kRules{{
    // stage_state: ERR-07 route=576, ERR-09 schema=2 ordinal=true, ERR-11 replay=vehicle
    {Structure::stage_state, 576, 2, IdPolicy::auto_gen, ReplayScope::vehicle, false, true, 0x01u, 2, Branch::command, 191, 219},
    // stage_states: ERR-08 id_policy=strict
    {Structure::stage_states, 415, 2, IdPolicy::strict, ReplayScope::none, true, false, 0x0fu, 1, Branch::event, 85, 201},
    // stage_state_view: ERR-05 branch=command, changelog phase=5
    {Structure::stage_state_view, 670, 5, IdPolicy::keep_blank, ReplayScope::stage, true, false, 0x35u, 5, Branch::command, 353, 784},
    // stage_state_patch: ERR-00 id_policy=keep_blank, ERR-09 ordinal=true
    {Structure::stage_state_patch, 329, 5, IdPolicy::keep_blank, ReplayScope::none, true, true, 0x2au, 1, Branch::config, 489, 1488},
    // stage_state_legacy: ERR-01 replay=range, ERR-09 ordinal=true
    {Structure::stage_state_legacy, 482, 9, IdPolicy::auto_gen, ReplayScope::range, true, true, 0x2cu, 2, Branch::event, 205, 1189},
    // stage_state_v2: ERR-09 schema=7 replay=range, ERR-11 stamp=false
    {Structure::stage_state_v2, 756, 7, IdPolicy::auto_gen, ReplayScope::range, false, true, 0x19u, 2, Branch::event, 75, 622},
    // thrust_report: ERR-02 route=385, ERR-04 ordinal=false, ERR-07 stamp=false
    {Structure::thrust_report, 385, 2, IdPolicy::strict, ReplayScope::range, false, false, 0x06u, 5, Branch::command, 23, 834},
    // thrust_reports: ERR-07 schema=2, ERR-09 id_policy=keep_blank replay=stage
    {Structure::thrust_reports, 482, 2, IdPolicy::keep_blank, ReplayScope::stage, false, false, 0x2fu, 1, Branch::telemetry, 449, 916},
    // thrust_report_view: ERR-08 band=[71,559], changelog id_policy=strict branch=event
    {Structure::thrust_report_view, 885, 9, IdPolicy::strict, ReplayScope::none, true, false, 0x1bu, 2, Branch::event, 71, 559},
    // thrust_report_patch: ERR-06 ordinal=true, ERR-07 stamp=true
    {Structure::thrust_report_patch, 465, 4, IdPolicy::keep_blank, ReplayScope::stage, true, true, 0x0fu, 3, Branch::command, 459, 983},
    // thrust_report_legacy: ERR-03 stamp=false
    {Structure::thrust_report_legacy, 863, 8, IdPolicy::auto_gen, ReplayScope::none, false, false, 0x08u, 6, Branch::event, 350, 545},
    // thrust_report_v2: ERR-01 ordinal=true, ERR-03 route=986 replay=vehicle, ERR-11 schema=7
    {Structure::thrust_report_v2, 986, 7, IdPolicy::keep_blank, ReplayScope::vehicle, true, true, 0x04u, 1, Branch::telemetry, 116, 519},
    // nav_solution: ERR-01 schema=4, ERR-03 replay=none, ERR-05 ordinal=true
    {Structure::nav_solution, 342, 4, IdPolicy::strict, ReplayScope::none, false, true, 0x3bu, 6, Branch::event, 290, 612},
    // nav_solutions: ERR-01 schema=8 id_policy=strict, ERR-08 ordinal=false
    {Structure::nav_solutions, 522, 8, IdPolicy::strict, ReplayScope::range, true, false, 0x0eu, 6, Branch::event, 408, 794},
    // nav_solution_view: ERR-03 band=[220,728], ERR-07 stamp=false, ERR-08 replay=stage, ERR-09 ordinal=true
    {Structure::nav_solution_view, 772, 5, IdPolicy::keep_blank, ReplayScope::stage, false, true, 0x36u, 1, Branch::telemetry, 220, 728},
    // nav_solution_patch: ERR-01 ordinal=false, ERR-06 stamp=false
    {Structure::nav_solution_patch, 798, 3, IdPolicy::auto_gen, ReplayScope::stage, false, false, 0x27u, 0, Branch::command, 98, 1018},
    // nav_solution_legacy: ERR-05 ordinal=false
    {Structure::nav_solution_legacy, 565, 8, IdPolicy::keep_blank, ReplayScope::range, false, false, 0x10u, 6, Branch::event, 414, 872},
    // nav_solution_v2: ERR-04 band=[351,716], ERR-09 schema=2, ERR-10 id_policy=keep_blank, ERR-11 ordinal=false
    {Structure::nav_solution_v2, 591, 2, IdPolicy::keep_blank, ReplayScope::vehicle, true, false, 0x04u, 0, Branch::event, 351, 716},
    // sep_event: ERR-04 band=[464,557], ERR-10 branch=config
    {Structure::sep_event, 869, 6, IdPolicy::strict, ReplayScope::stage, false, false, 0x09u, 5, Branch::config, 464, 557},
    // sep_events: ERR-01 id_policy=keep_blank, ERR-02 schema=8, ERR-04 ordinal=false, ERR-05 branch=event, ERR-09 replay=range
    {Structure::sep_events, 277, 8, IdPolicy::keep_blank, ReplayScope::range, true, false, 0x09u, 2, Branch::event, 85, 931},
    // sep_event_view: ERR-04 band=[341,1082], ERR-11 schema=9 stamp=false
    {Structure::sep_event_view, 981, 9, IdPolicy::auto_gen, ReplayScope::vehicle, false, true, 0x20u, 0, Branch::config, 341, 1082},
    // sep_event_patch: ERR-04 branch=config, ERR-10 ordinal=false
    {Structure::sep_event_patch, 515, 4, IdPolicy::auto_gen, ReplayScope::none, false, false, 0x3bu, 0, Branch::config, 135, 814},
    // sep_event_legacy: ERR-09 replay=range, ERR-10 ordinal=false, ERR-11 schema=4
    {Structure::sep_event_legacy, 224, 4, IdPolicy::keep_blank, ReplayScope::range, true, false, 0x3au, 0, Branch::event, 136, 432},
    // sep_event_v2: ERR-01 replay=vehicle, ERR-03 branch=telemetry, ERR-11 schema=6
    {Structure::sep_event_v2, 552, 6, IdPolicy::strict, ReplayScope::vehicle, false, false, 0x1fu, 3, Branch::telemetry, 42, 259},
    // fts_status: ERR-02 band=[308,432], ERR-08 stamp=false
    {Structure::fts_status, 635, 6, IdPolicy::strict, ReplayScope::none, false, false, 0x04u, 0, Branch::event, 308, 432},
    // fts_statuses: ERR-06 schema=3, ERR-09 replay=vehicle, ERR-11 ordinal=false
    {Structure::fts_statuses, 856, 3, IdPolicy::strict, ReplayScope::vehicle, true, false, 0x16u, 3, Branch::telemetry, 328, 549},
    // fts_status_view: ERR-03 branch=telemetry, ERR-07 stamp=false
    {Structure::fts_status_view, 209, 2, IdPolicy::keep_blank, ReplayScope::vehicle, false, true, 0x14u, 1, Branch::telemetry, 445, 721},
    // fts_status_patch: ERR-05 id_policy=auto_gen, ERR-07 replay=none, ERR-10 stamp=false
    {Structure::fts_status_patch, 825, 9, IdPolicy::auto_gen, ReplayScope::none, false, false, 0x03u, 0, Branch::event, 2, 390},
    // fts_status_legacy: 无勘误
    {Structure::fts_status_legacy, 122, 6, IdPolicy::auto_gen, ReplayScope::stage, false, false, 0x26u, 1, Branch::event, 209, 233},
    // fts_status_v2: ERR-09 replay=stage
    {Structure::fts_status_v2, 110, 5, IdPolicy::auto_gen, ReplayScope::stage, false, false, 0x30u, 3, Branch::event, 167, 454},
    // payload_frame: 无勘误
    {Structure::payload_frame, 810, 7, IdPolicy::keep_blank, ReplayScope::vehicle, true, false, 0x33u, 5, Branch::telemetry, 142, 766},
    // payload_frames: ERR-09 replay=range
    {Structure::payload_frames, 425, 4, IdPolicy::strict, ReplayScope::range, false, false, 0x1fu, 2, Branch::telemetry, 277, 1162},
    // payload_frame_view: ERR-02 id_policy=keep_blank ordinal=true, ERR-04 route=972
    {Structure::payload_frame_view, 972, 9, IdPolicy::keep_blank, ReplayScope::range, true, true, 0x2bu, 3, Branch::event, 481, 1029},
    // payload_frame_patch: ERR-01 replay=range, ERR-02 branch=command, ERR-06 route=667, ERR-10 stamp=false, ERR-11 band=[484,812]
    {Structure::payload_frame_patch, 667, 7, IdPolicy::auto_gen, ReplayScope::range, false, true, 0x11u, 0, Branch::command, 484, 812},
    // payload_frame_legacy: ERR-02 stamp=true, ERR-07 id_policy=strict, ERR-10 branch=event, ERR-11 schema=1
    {Structure::payload_frame_legacy, 458, 1, IdPolicy::strict, ReplayScope::range, true, true, 0x08u, 1, Branch::event, 333, 700},
    // payload_frame_v2: ERR-01 replay=stage ordinal=true, ERR-08 schema=6, ERR-09 band=[413,705]
    {Structure::payload_frame_v2, 803, 6, IdPolicy::keep_blank, ReplayScope::stage, false, true, 0x1fu, 1, Branch::telemetry, 413, 705},
    // link_quality: ERR-00 ordinal=false, ERR-03 branch=telemetry, ERR-06 route=412
    {Structure::link_quality, 412, 9, IdPolicy::strict, ReplayScope::range, true, false, 0x39u, 5, Branch::telemetry, 158, 248},
    // link_qualities: ERR-06 branch=command, ERR-10 id_policy=keep_blank
    {Structure::link_qualities, 761, 9, IdPolicy::keep_blank, ReplayScope::range, false, false, 0x33u, 5, Branch::command, 81, 926},
    // link_quality_view: ERR-03 id_policy=auto_gen, ERR-10 replay=none
    {Structure::link_quality_view, 517, 9, IdPolicy::auto_gen, ReplayScope::none, true, true, 0x0cu, 0, Branch::command, 122, 533},
    // link_quality_patch: ERR-08 stamp=false, ERR-09 id_policy=keep_blank ordinal=false
    {Structure::link_quality_patch, 713, 1, IdPolicy::keep_blank, ReplayScope::range, false, false, 0x26u, 5, Branch::config, 390, 865},
    // link_quality_legacy: ERR-10 ordinal=true
    {Structure::link_quality_legacy, 782, 2, IdPolicy::keep_blank, ReplayScope::none, false, true, 0x37u, 5, Branch::command, 185, 592},
    // link_quality_v2: ERR-03 id_policy=strict, ERR-04 band=[374,931], ERR-10 schema=4 stamp=true
    {Structure::link_quality_v2, 773, 4, IdPolicy::strict, ReplayScope::range, true, false, 0x39u, 1, Branch::event, 374, 931},
    // thermal_sample: ERR-04 ordinal=true, ERR-10 branch=event schema=7
    {Structure::thermal_sample, 100, 7, IdPolicy::keep_blank, ReplayScope::vehicle, true, true, 0x12u, 5, Branch::event, 495, 883},
    // thermal_samples: ERR-06 replay=stage stamp=true, ERR-09 route=978
    {Structure::thermal_samples, 978, 6, IdPolicy::auto_gen, ReplayScope::stage, true, true, 0x10u, 2, Branch::telemetry, 133, 255},
    // thermal_sample_view: ERR-02 replay=none ordinal=true, ERR-08 schema=9 id_policy=keep_blank, ERR-09 stamp=false
    {Structure::thermal_sample_view, 520, 9, IdPolicy::keep_blank, ReplayScope::none, false, true, 0x08u, 2, Branch::event, 406, 539},
    // thermal_sample_patch: ERR-02 ordinal=true, ERR-07 stamp=true
    {Structure::thermal_sample_patch, 197, 5, IdPolicy::strict, ReplayScope::vehicle, true, true, 0x06u, 0, Branch::command, 324, 475},
    // thermal_sample_legacy: ERR-09 schema=9
    {Structure::thermal_sample_legacy, 130, 9, IdPolicy::keep_blank, ReplayScope::range, false, true, 0x34u, 2, Branch::config, 466, 1209},
    // thermal_sample_v2: ERR-05 branch=config
    {Structure::thermal_sample_v2, 819, 1, IdPolicy::strict, ReplayScope::none, true, false, 0x20u, 5, Branch::config, 286, 669},
}};

}  // 匿名命名空间

std::optional<FieldRule> rule_for(Structure id) {
    for (const FieldRule& r : kRules) {
        if (r.id == id) return r;
    }
    return std::nullopt;
}

bool accepts(Structure id, const Incoming& in) {
    const auto r = rule_for(id);
    if (!r) return false;
    if (r->id_policy == IdPolicy::strict && in.identifier.empty()) return false;
    if (r->stamp_required && !in.has_stamp) return false;
    if (r->ordinal_required && !in.has_ordinal) return false;
    return true;
}

bool replayable(Structure id) {
    const auto r = rule_for(id);
    return r && r->replay_scope != ReplayScope::none;
}

bool flag_active(Structure id, ModeBit bit) {
    const auto r = rule_for(id);
    if (!r) return false;
    return (r->mode_flag & (std::uint32_t{1} << static_cast<unsigned>(bit))) != 0;
}

int phase_rank(Structure id) {
    const auto r = rule_for(id);
    return r ? static_cast<int>(r->phase_value) : -1;
}

Branch branch_of(Structure id) {
    const auto r = rule_for(id);
    return r ? r->branch : Branch::telemetry;
}

bool within_band(Structure id, std::int32_t sample) {
    const auto r = rule_for(id);
    if (!r) return false;
    return sample >= r->band_min && sample <= r->band_max;
}
