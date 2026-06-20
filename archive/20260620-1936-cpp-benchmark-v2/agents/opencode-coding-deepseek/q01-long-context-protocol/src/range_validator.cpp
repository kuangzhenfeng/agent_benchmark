#include "range_validator.hpp"

#include <array>

namespace {

// v6 真值规则表。按 ADR-009 优先级（勘误表 > changelog > v6.proto 注释 > 字段词典 > 历史代码）交叉核对。
// 每个 FieldRule 包含 10 个维度：路由码、小版本、标识策略、回放域、
// 是否需要时间戳、是否需要序号、六位模式位、飞行阶段编号、载荷分支、合法区间。
// v6 阶段编号：IDLE=0 STANDBY=1 BOOST=2 ASCENT=3 RESERVED=4 COAST=5 INSERTION=6
constexpr std::array<FieldRule, 48> kRules{{
    {Structure::stage_state, 576, 2, IdPolicy::auto_gen, ReplayScope::vehicle, false, true, 0x01u, 2, Branch::command, 191, 219},
    {Structure::stage_states, 415, 2, IdPolicy::strict, ReplayScope::none, true, false, 0x0fu, 1, Branch::event, 85, 201},
    {Structure::stage_state_view, 670, 5, IdPolicy::keep_blank, ReplayScope::stage, true, false, 0x35u, 5, Branch::command, 353, 784},
    {Structure::stage_state_patch, 329, 5, IdPolicy::keep_blank, ReplayScope::none, true, true, 0x2au, 1, Branch::config, 489, 1488},
    {Structure::stage_state_legacy, 482, 9, IdPolicy::auto_gen, ReplayScope::range, true, true, 0x2cu, 2, Branch::event, 205, 1189},
    {Structure::stage_state_v2, 756, 7, IdPolicy::auto_gen, ReplayScope::range, false, true, 0x19u, 2, Branch::event, 75, 622},
    {Structure::thrust_report, 385, 2, IdPolicy::strict, ReplayScope::range, false, false, 0x06u, 5, Branch::command, 23, 834},
    {Structure::thrust_reports, 482, 2, IdPolicy::keep_blank, ReplayScope::stage, false, false, 0x2fu, 1, Branch::telemetry, 449, 916},
    {Structure::thrust_report_view, 885, 9, IdPolicy::strict, ReplayScope::none, true, false, 0x1bu, 2, Branch::event, 71, 559},
    {Structure::thrust_report_patch, 465, 4, IdPolicy::keep_blank, ReplayScope::stage, true, true, 0x0fu, 3, Branch::command, 459, 983},
    {Structure::thrust_report_legacy, 352, 8, IdPolicy::auto_gen, ReplayScope::none, false, false, 0x08u, 6, Branch::event, 350, 545},
    {Structure::thrust_report_v2, 986, 7, IdPolicy::keep_blank, ReplayScope::vehicle, true, true, 0x04u, 1, Branch::telemetry, 116, 519},
    {Structure::nav_solution, 342, 4, IdPolicy::strict, ReplayScope::none, false, true, 0x3bu, 6, Branch::event, 290, 612},
    {Structure::nav_solutions, 522, 8, IdPolicy::strict, ReplayScope::range, true, false, 0x0eu, 6, Branch::event, 408, 794},
    {Structure::nav_solution_view, 892, 5, IdPolicy::keep_blank, ReplayScope::stage, false, true, 0x36u, 1, Branch::telemetry, 220, 728},
    {Structure::nav_solution_patch, 798, 3, IdPolicy::auto_gen, ReplayScope::stage, false, false, 0x27u, 0, Branch::command, 98, 1018},
    {Structure::nav_solution_legacy, 565, 8, IdPolicy::keep_blank, ReplayScope::range, false, false, 0x10u, 6, Branch::event, 414, 872},
    {Structure::nav_solution_v2, 591, 2, IdPolicy::keep_blank, ReplayScope::vehicle, true, false, 0x04u, 0, Branch::event, 351, 716},
    {Structure::sep_event, 869, 6, IdPolicy::strict, ReplayScope::stage, false, false, 0x09u, 5, Branch::config, 464, 557},
    {Structure::sep_events, 277, 8, IdPolicy::keep_blank, ReplayScope::range, true, false, 0x09u, 2, Branch::event, 85, 931},
    {Structure::sep_event_view, 981, 9, IdPolicy::auto_gen, ReplayScope::vehicle, false, true, 0x20u, 0, Branch::config, 341, 1082},
    {Structure::sep_event_patch, 515, 4, IdPolicy::auto_gen, ReplayScope::none, false, false, 0x3bu, 0, Branch::config, 135, 814},
    {Structure::sep_event_legacy, 224, 4, IdPolicy::keep_blank, ReplayScope::range, true, false, 0x3au, 0, Branch::event, 136, 432},
    {Structure::sep_event_v2, 552, 6, IdPolicy::strict, ReplayScope::vehicle, false, false, 0x1fu, 3, Branch::telemetry, 42, 259},
    {Structure::fts_status, 635, 6, IdPolicy::strict, ReplayScope::none, false, false, 0x04u, 0, Branch::event, 308, 432},
    {Structure::fts_statuses, 856, 3, IdPolicy::strict, ReplayScope::vehicle, true, false, 0x16u, 3, Branch::telemetry, 328, 549},
    {Structure::fts_status_view, 209, 2, IdPolicy::keep_blank, ReplayScope::vehicle, false, true, 0x14u, 1, Branch::telemetry, 445, 721},
    {Structure::fts_status_patch, 825, 9, IdPolicy::auto_gen, ReplayScope::none, false, false, 0x03u, 0, Branch::event, 2, 390},
    {Structure::fts_status_legacy, 122, 6, IdPolicy::auto_gen, ReplayScope::stage, false, false, 0x26u, 1, Branch::event, 209, 233},
    {Structure::fts_status_v2, 110, 5, IdPolicy::auto_gen, ReplayScope::stage, false, false, 0x30u, 3, Branch::event, 167, 454},
    {Structure::payload_frame, 810, 7, IdPolicy::keep_blank, ReplayScope::vehicle, true, false, 0x33u, 5, Branch::telemetry, 142, 766},
    {Structure::payload_frames, 425, 4, IdPolicy::strict, ReplayScope::range, false, false, 0x1fu, 0, Branch::telemetry, 277, 1162},
    {Structure::payload_frame_view, 972, 9, IdPolicy::keep_blank, ReplayScope::range, true, true, 0x2bu, 3, Branch::event, 481, 1029},
    {Structure::payload_frame_patch, 667, 7, IdPolicy::auto_gen, ReplayScope::range, false, true, 0x11u, 0, Branch::command, 484, 812},
    {Structure::payload_frame_legacy, 458, 1, IdPolicy::strict, ReplayScope::range, true, true, 0x08u, 1, Branch::event, 333, 700},
    {Structure::payload_frame_v2, 803, 6, IdPolicy::keep_blank, ReplayScope::stage, false, true, 0x1fu, 1, Branch::telemetry, 413, 705},
    {Structure::link_quality, 412, 9, IdPolicy::strict, ReplayScope::range, true, false, 0x39u, 5, Branch::telemetry, 158, 248},
    {Structure::link_qualities, 761, 9, IdPolicy::keep_blank, ReplayScope::range, false, false, 0x33u, 5, Branch::command, 81, 926},
    {Structure::link_quality_view, 517, 9, IdPolicy::auto_gen, ReplayScope::none, true, true, 0x0cu, 3, Branch::command, 122, 533},
    {Structure::link_quality_patch, 713, 1, IdPolicy::keep_blank, ReplayScope::range, false, false, 0x26u, 5, Branch::config, 390, 865},
    {Structure::link_quality_legacy, 782, 2, IdPolicy::keep_blank, ReplayScope::none, false, true, 0x37u, 5, Branch::command, 185, 592},
    {Structure::link_quality_v2, 773, 4, IdPolicy::strict, ReplayScope::range, true, false, 0x39u, 1, Branch::event, 374, 931},
    {Structure::thermal_sample, 100, 7, IdPolicy::keep_blank, ReplayScope::vehicle, true, true, 0x12u, 5, Branch::event, 495, 883},
    {Structure::thermal_samples, 978, 6, IdPolicy::auto_gen, ReplayScope::stage, true, true, 0x10u, 2, Branch::telemetry, 133, 255},
    {Structure::thermal_sample_view, 520, 9, IdPolicy::keep_blank, ReplayScope::none, false, true, 0x08u, 2, Branch::event, 406, 539},
    {Structure::thermal_sample_patch, 197, 5, IdPolicy::strict, ReplayScope::vehicle, true, true, 0x06u, 0, Branch::command, 324, 475},
    {Structure::thermal_sample_legacy, 130, 9, IdPolicy::keep_blank, ReplayScope::range, false, true, 0x34u, 2, Branch::config, 466, 1209},
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