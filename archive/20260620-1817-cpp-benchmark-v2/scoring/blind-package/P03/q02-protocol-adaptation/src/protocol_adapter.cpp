#include "protocol_adapter.hpp"

#include <array>

namespace {

// 规则表: 系统 B 中 48 个实现绑定结构的最终值。
constexpr std::array<FieldRule, 48> kRules{{
    {Structure::stage_state, 765, 5, IdPolicy::keep_blank, ReplayScope::vehicle, true, true, 0x0bu, 3, Branch::config, 313, 925},
    {Structure::stage_states, 924, 7, IdPolicy::keep_blank, ReplayScope::none, true, true, 0x1du, 0, Branch::event, 295, 1082},
    {Structure::stage_state_view, 960, 7, IdPolicy::keep_blank, ReplayScope::range, false, true, 0x3eu, 2, Branch::command, 348, 777},
    {Structure::stage_state_patch, 694, 5, IdPolicy::auto_gen, ReplayScope::vehicle, false, true, 0x02u, 0, Branch::event, 54, 739},
    {Structure::stage_state_legacy, 105, 7, IdPolicy::keep_blank, ReplayScope::vehicle, true, false, 0x21u, 1, Branch::event, 389, 498},
    {Structure::stage_state_v2, 421, 5, IdPolicy::keep_blank, ReplayScope::vehicle, true, false, 0x17u, 3, Branch::telemetry, 0, 112},
    {Structure::thrust_report, 709, 8, IdPolicy::keep_blank, ReplayScope::stage, true, true, 0x01u, 1, Branch::event, 362, 486},
    {Structure::thrust_reports, 521, 2, IdPolicy::strict, ReplayScope::none, true, false, 0x30u, 0, Branch::event, 202, 452},
    {Structure::thrust_report_view, 389, 8, IdPolicy::strict, ReplayScope::vehicle, true, false, 0x21u, 5, Branch::event, 30, 435},
    {Structure::thrust_report_patch, 216, 7, IdPolicy::keep_blank, ReplayScope::vehicle, false, false, 0x10u, 0, Branch::config, 333, 1120},
    {Structure::thrust_report_legacy, 242, 9, IdPolicy::auto_gen, ReplayScope::vehicle, true, false, 0x0eu, 6, Branch::config, 196, 864},
    {Structure::thrust_report_v2, 453, 2, IdPolicy::keep_blank, ReplayScope::vehicle, true, false, 0x33u, 5, Branch::config, 89, 811},
    {Structure::nav_solution, 700, 2, IdPolicy::auto_gen, ReplayScope::none, true, false, 0x10u, 1, Branch::command, 115, 1083},
    {Structure::nav_solutions, 583, 5, IdPolicy::keep_blank, ReplayScope::none, true, true, 0x0au, 0, Branch::command, 158, 406},
    {Structure::nav_solution_view, 147, 8, IdPolicy::keep_blank, ReplayScope::none, false, false, 0x06u, 1, Branch::command, 67, 738},
    {Structure::nav_solution_patch, 838, 2, IdPolicy::keep_blank, ReplayScope::none, false, true, 0x03u, 5, Branch::event, 419, 805},
    {Structure::nav_solution_legacy, 617, 2, IdPolicy::keep_blank, ReplayScope::none, true, true, 0x36u, 1, Branch::command, 241, 796},
    {Structure::nav_solution_v2, 701, 8, IdPolicy::keep_blank, ReplayScope::none, true, true, 0x0bu, 0, Branch::command, 237, 785},
    {Structure::sep_event, 786, 5, IdPolicy::auto_gen, ReplayScope::range, false, true, 0x1au, 6, Branch::command, 321, 610},
    {Structure::sep_events, 625, 8, IdPolicy::auto_gen, ReplayScope::range, true, false, 0x3bu, 1, Branch::command, 194, 1044},
    {Structure::sep_event_view, 867, 6, IdPolicy::auto_gen, ReplayScope::none, true, false, 0x3du, 2, Branch::command, 253, 1129},
    {Structure::sep_event_patch, 601, 5, IdPolicy::keep_blank, ReplayScope::none, true, true, 0x1eu, 2, Branch::config, 31, 311},
    {Structure::sep_event_legacy, 427, 4, IdPolicy::keep_blank, ReplayScope::vehicle, false, false, 0x23u, 3, Branch::telemetry, 381, 1049},
    {Structure::sep_event_v2, 644, 6, IdPolicy::strict, ReplayScope::stage, true, false, 0x2bu, 1, Branch::event, 482, 1430},
    {Structure::fts_status, 103, 7, IdPolicy::strict, ReplayScope::none, false, true, 0x1au, 0, Branch::config, 25, 714},
    {Structure::fts_statuses, 289, 3, IdPolicy::auto_gen, ReplayScope::none, true, false, 0x35u, 5, Branch::event, 17, 885},
    {Structure::fts_status_view, 915, 4, IdPolicy::auto_gen, ReplayScope::stage, false, true, 0x22u, 2, Branch::telemetry, 223, 497},
    {Structure::fts_status_patch, 809, 9, IdPolicy::keep_blank, ReplayScope::none, false, true, 0x07u, 5, Branch::command, 389, 955},
    {Structure::fts_status_legacy, 476, 5, IdPolicy::auto_gen, ReplayScope::vehicle, true, true, 0x0fu, 2, Branch::config, 103, 1073},
    {Structure::fts_status_v2, 429, 5, IdPolicy::auto_gen, ReplayScope::vehicle, true, true, 0x22u, 1, Branch::event, 19, 159},
    {Structure::payload_frame, 108, 5, IdPolicy::auto_gen, ReplayScope::vehicle, false, true, 0x2du, 5, Branch::telemetry, 363, 994},
    {Structure::payload_frames, 593, 7, IdPolicy::auto_gen, ReplayScope::range, true, true, 0x04u, 1, Branch::config, 419, 832},
    {Structure::payload_frame_view, 175, 9, IdPolicy::strict, ReplayScope::vehicle, true, true, 0x03u, 0, Branch::telemetry, 99, 880},
    {Structure::payload_frame_patch, 697, 5, IdPolicy::keep_blank, ReplayScope::none, true, true, 0x21u, 3, Branch::command, 153, 173},
    {Structure::payload_frame_legacy, 792, 5, IdPolicy::keep_blank, ReplayScope::stage, true, false, 0x03u, 1, Branch::telemetry, 202, 630},
    {Structure::payload_frame_v2, 255, 1, IdPolicy::strict, ReplayScope::range, false, true, 0x1fu, 0, Branch::event, 36, 463},
    {Structure::link_quality, 239, 7, IdPolicy::strict, ReplayScope::range, true, true, 0x28u, 1, Branch::config, 125, 249},
    {Structure::link_qualities, 683, 6, IdPolicy::keep_blank, ReplayScope::vehicle, false, false, 0x01u, 2, Branch::config, 471, 791},
    {Structure::link_quality_view, 509, 9, IdPolicy::strict, ReplayScope::range, false, true, 0x03u, 0, Branch::command, 136, 428},
    {Structure::link_quality_patch, 386, 8, IdPolicy::auto_gen, ReplayScope::none, true, true, 0x2eu, 2, Branch::config, 220, 302},
    {Structure::link_quality_legacy, 424, 8, IdPolicy::keep_blank, ReplayScope::vehicle, true, true, 0x3du, 5, Branch::telemetry, 277, 1022},
    {Structure::link_quality_v2, 923, 1, IdPolicy::strict, ReplayScope::stage, true, true, 0x3bu, 0, Branch::config, 303, 1110},
    {Structure::thermal_sample, 454, 2, IdPolicy::strict, ReplayScope::range, false, true, 0x17u, 1, Branch::command, 302, 608},
    {Structure::thermal_samples, 127, 5, IdPolicy::keep_blank, ReplayScope::vehicle, true, false, 0x3cu, 5, Branch::telemetry, 214, 613},
    {Structure::thermal_sample_view, 716, 3, IdPolicy::auto_gen, ReplayScope::range, true, false, 0x3bu, 0, Branch::config, 330, 629},
    {Structure::thermal_sample_patch, 821, 7, IdPolicy::auto_gen, ReplayScope::none, true, true, 0x02u, 3, Branch::telemetry, 447, 1033},
    {Structure::thermal_sample_legacy, 749, 4, IdPolicy::strict, ReplayScope::none, false, false, 0x1au, 3, Branch::event, 494, 908},
    {Structure::thermal_sample_v2, 316, 3, IdPolicy::strict, ReplayScope::stage, false, true, 0x02u, 2, Branch::event, 121, 234},
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
    // 仅"严格必填"拒绝空标识; 保留空值与空则生成均接受。
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
    const std::uint32_t mask = std::uint32_t{1} << static_cast<unsigned>(bit);
    return (r->mode_flag & mask) != 0;
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
