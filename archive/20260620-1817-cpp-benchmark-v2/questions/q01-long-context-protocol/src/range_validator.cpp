#include "range_validator.hpp"

#include <array>

namespace {

// 规则表: 当前为【未读全语料的错误直觉值】。
// 参评 Agent 须结合 corpus 多份文档, 按 ADR-009 优先级校正为 v6 真值。
constexpr std::array<FieldRule, 48> kRules{{
    {Structure::stage_state, 230, 5, IdPolicy::auto_gen, ReplayScope::none, false, false, 0x04u, 2, Branch::command, 191, 219},
    {Structure::stage_states, 548, 2, IdPolicy::keep_blank, ReplayScope::range, true, false, 0x2du, 6, Branch::event, 85, 201},
    {Structure::stage_state_view, 670, 5, IdPolicy::keep_blank, ReplayScope::stage, true, false, 0x35u, 1, Branch::event, 353, 784},
    {Structure::stage_state_patch, 329, 5, IdPolicy::strict, ReplayScope::none, true, false, 0x24u, 2, Branch::config, 61, 58},
    {Structure::stage_state_legacy, 482, 9, IdPolicy::auto_gen, ReplayScope::stage, true, false, 0x1cu, 2, Branch::event, 205, 1189},
    {Structure::stage_state_v2, 756, 1, IdPolicy::auto_gen, ReplayScope::stage, true, true, 0x19u, 2, Branch::event, 75, 622},
    {Structure::thrust_report, 973, 2, IdPolicy::strict, ReplayScope::vehicle, true, true, 0x06u, 6, Branch::command, 234, 202},
    {Structure::thrust_reports, 482, 8, IdPolicy::auto_gen, ReplayScope::range, false, false, 0x2fu, 5, Branch::telemetry, 449, 916},
    {Structure::thrust_report_view, 885, 9, IdPolicy::keep_blank, ReplayScope::none, true, false, 0x3bu, 2, Branch::telemetry, 134, 409},
    {Structure::thrust_report_patch, 397, 4, IdPolicy::auto_gen, ReplayScope::stage, false, false, 0x12u, 1, Branch::event, 459, 983},
    {Structure::thrust_report_legacy, 863, 8, IdPolicy::auto_gen, ReplayScope::none, true, false, 0x08u, 1, Branch::event, 350, 545},
    {Structure::thrust_report_v2, 821, 6, IdPolicy::keep_blank, ReplayScope::stage, true, false, 0x04u, 1, Branch::telemetry, 116, 519},
    {Structure::nav_solution, 342, 9, IdPolicy::keep_blank, ReplayScope::stage, false, false, 0x37u, 6, Branch::event, 290, 612},
    {Structure::nav_solutions, 522, 2, IdPolicy::keep_blank, ReplayScope::range, true, true, 0x08u, 3, Branch::event, 408, 794},
    {Structure::nav_solution_view, 772, 5, IdPolicy::keep_blank, ReplayScope::vehicle, true, false, 0x08u, 1, Branch::telemetry, 181, 84},
    {Structure::nav_solution_patch, 798, 3, IdPolicy::auto_gen, ReplayScope::none, true, true, 0x27u, 5, Branch::command, 98, 1018},
    {Structure::nav_solution_legacy, 565, 8, IdPolicy::auto_gen, ReplayScope::range, false, true, 0x10u, 0, Branch::event, 165, 199},
    {Structure::nav_solution_v2, 591, 8, IdPolicy::auto_gen, ReplayScope::vehicle, true, true, 0x10u, 1, Branch::event, 186, 123},
    {Structure::sep_event, 869, 6, IdPolicy::strict, ReplayScope::stage, false, false, 0x37u, 5, Branch::command, 98, 345},
    {Structure::sep_events, 277, 1, IdPolicy::auto_gen, ReplayScope::vehicle, true, true, 0x04u, 0, Branch::config, 370, 66},
    {Structure::sep_event_view, 981, 2, IdPolicy::auto_gen, ReplayScope::vehicle, true, true, 0x20u, 1, Branch::telemetry, 414, 339},
    {Structure::sep_event_patch, 520, 4, IdPolicy::auto_gen, ReplayScope::none, false, true, 0x3bu, 1, Branch::command, 135, 814},
    {Structure::sep_event_legacy, 644, 1, IdPolicy::keep_blank, ReplayScope::stage, true, true, 0x3au, 2, Branch::telemetry, 136, 432},
    {Structure::sep_event_v2, 552, 1, IdPolicy::auto_gen, ReplayScope::range, false, false, 0x08u, 3, Branch::config, 42, 259},
    {Structure::fts_status, 556, 6, IdPolicy::auto_gen, ReplayScope::none, true, false, 0x36u, 0, Branch::event, 110, 285},
    {Structure::fts_statuses, 256, 2, IdPolicy::strict, ReplayScope::stage, true, true, 0x16u, 3, Branch::telemetry, 50, 73},
    {Structure::fts_status_view, 209, 2, IdPolicy::keep_blank, ReplayScope::vehicle, true, true, 0x24u, 1, Branch::command, 445, 721},
    {Structure::fts_status_patch, 802, 9, IdPolicy::strict, ReplayScope::stage, true, false, 0x03u, 5, Branch::event, 2, 390},
    {Structure::fts_status_legacy, 122, 6, IdPolicy::strict, ReplayScope::stage, false, false, 0x26u, 2, Branch::event, 209, 233},
    {Structure::fts_status_v2, 110, 5, IdPolicy::auto_gen, ReplayScope::range, false, false, 0x32u, 3, Branch::config, 167, 454},
    {Structure::payload_frame, 660, 7, IdPolicy::keep_blank, ReplayScope::vehicle, true, false, 0x37u, 2, Branch::telemetry, 279, 137},
    {Structure::payload_frames, 425, 4, IdPolicy::keep_blank, ReplayScope::none, false, false, 0x20u, 2, Branch::telemetry, 277, 1162},
    {Structure::payload_frame_view, 175, 9, IdPolicy::strict, ReplayScope::range, true, false, 0x18u, 3, Branch::event, 42, 134},
    {Structure::payload_frame_patch, 459, 7, IdPolicy::auto_gen, ReplayScope::vehicle, true, true, 0x23u, 0, Branch::telemetry, 316, 463},
    {Structure::payload_frame_legacy, 458, 8, IdPolicy::auto_gen, ReplayScope::stage, false, true, 0x08u, 2, Branch::command, 389, 79},
    {Structure::payload_frame_v2, 803, 4, IdPolicy::keep_blank, ReplayScope::vehicle, false, false, 0x1bu, 1, Branch::telemetry, 300, 295},
    {Structure::link_quality, 664, 9, IdPolicy::strict, ReplayScope::range, true, true, 0x24u, 5, Branch::config, 158, 248},
    {Structure::link_qualities, 994, 9, IdPolicy::strict, ReplayScope::none, false, false, 0x37u, 5, Branch::config, 81, 926},
    {Structure::link_quality_view, 246, 9, IdPolicy::strict, ReplayScope::range, true, true, 0x08u, 0, Branch::telemetry, 122, 533},
    {Structure::link_quality_patch, 713, 1, IdPolicy::strict, ReplayScope::range, true, true, 0x26u, 5, Branch::config, 222, 99},
    {Structure::link_quality_legacy, 782, 2, IdPolicy::keep_blank, ReplayScope::none, false, false, 0x37u, 5, Branch::command, 185, 592},
    {Structure::link_quality_v2, 773, 7, IdPolicy::keep_blank, ReplayScope::range, false, false, 0x39u, 1, Branch::event, 27, 160},
    {Structure::thermal_sample, 611, 6, IdPolicy::keep_blank, ReplayScope::vehicle, true, false, 0x3du, 2, Branch::config, 337, 453},
    {Structure::thermal_samples, 803, 6, IdPolicy::auto_gen, ReplayScope::vehicle, false, true, 0x37u, 2, Branch::telemetry, 360, 227},
    {Structure::thermal_sample_view, 557, 1, IdPolicy::strict, ReplayScope::range, true, false, 0x08u, 2, Branch::event, 449, 135},
    {Structure::thermal_sample_patch, 558, 5, IdPolicy::strict, ReplayScope::range, false, false, 0x18u, 0, Branch::command, 324, 475},
    {Structure::thermal_sample_legacy, 130, 5, IdPolicy::auto_gen, ReplayScope::range, false, true, 0x10u, 2, Branch::command, 466, 1209},
    {Structure::thermal_sample_v2, 819, 1, IdPolicy::strict, ReplayScope::none, true, false, 0x20u, 2, Branch::telemetry, 263, 270},
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
