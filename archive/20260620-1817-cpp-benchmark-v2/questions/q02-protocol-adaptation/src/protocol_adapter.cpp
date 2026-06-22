#include "protocol_adapter.hpp"

#include <array>

namespace {

// 规则表: 当前为【系统 A 的已完成值】(迁移前)。参评 Agent 须改为系统 B 的最终值。
constexpr std::array<FieldRule, 48> kRules{{
    {Structure::stage_state, 201, 5, IdPolicy::auto_gen, ReplayScope::stage, false, false, 0x2cu, 0, Branch::command, 313, 925},
    {Structure::stage_states, 695, 8, IdPolicy::strict, ReplayScope::range, true, false, 0x28u, 3, Branch::event, 148, 972},
    {Structure::stage_state_view, 251, 6, IdPolicy::auto_gen, ReplayScope::range, true, true, 0x04u, 4, Branch::config, 147, 621},
    {Structure::stage_state_patch, 132, 2, IdPolicy::strict, ReplayScope::none, true, false, 0x38u, 0, Branch::event, 248, 1006},
    {Structure::stage_state_legacy, 112, 3, IdPolicy::auto_gen, ReplayScope::range, false, false, 0x22u, 1, Branch::event, 178, 790},
    {Structure::stage_state_v2, 808, 3, IdPolicy::keep_blank, ReplayScope::vehicle, false, true, 0x34u, 1, Branch::event, 486, 1427},
    {Structure::thrust_report, 160, 4, IdPolicy::auto_gen, ReplayScope::vehicle, true, true, 0x37u, 1, Branch::command, 12, 197},
    {Structure::thrust_reports, 379, 7, IdPolicy::strict, ReplayScope::vehicle, false, true, 0x08u, 4, Branch::event, 185, 464},
    {Structure::thrust_report_view, 521, 8, IdPolicy::auto_gen, ReplayScope::none, true, true, 0x21u, 0, Branch::config, 393, 1065},
    {Structure::thrust_report_patch, 164, 7, IdPolicy::auto_gen, ReplayScope::vehicle, true, false, 0x39u, 4, Branch::config, 333, 1120},
    {Structure::thrust_report_legacy, 402, 9, IdPolicy::keep_blank, ReplayScope::range, true, true, 0x14u, 3, Branch::command, 196, 774},
    {Structure::thrust_report_v2, 178, 2, IdPolicy::auto_gen, ReplayScope::vehicle, true, true, 0x33u, 5, Branch::telemetry, 68, 219},
    {Structure::nav_solution, 506, 3, IdPolicy::keep_blank, ReplayScope::stage, false, true, 0x39u, 5, Branch::command, 291, 346},
    {Structure::nav_solutions, 444, 7, IdPolicy::auto_gen, ReplayScope::vehicle, true, false, 0x11u, 0, Branch::telemetry, 431, 1342},
    {Structure::nav_solution_view, 208, 8, IdPolicy::strict, ReplayScope::stage, true, true, 0x30u, 0, Branch::telemetry, 327, 768},
    {Structure::nav_solution_patch, 864, 9, IdPolicy::auto_gen, ReplayScope::stage, true, true, 0x03u, 2, Branch::command, 459, 624},
    {Structure::nav_solution_legacy, 152, 2, IdPolicy::strict, ReplayScope::stage, false, false, 0x3bu, 1, Branch::event, 356, 684},
    {Structure::nav_solution_v2, 961, 5, IdPolicy::auto_gen, ReplayScope::stage, false, false, 0x19u, 3, Branch::command, 237, 785},
    {Structure::sep_event, 998, 5, IdPolicy::keep_blank, ReplayScope::range, true, false, 0x31u, 1, Branch::event, 240, 369},
    {Structure::sep_events, 440, 5, IdPolicy::strict, ReplayScope::vehicle, true, true, 0x2eu, 5, Branch::command, 359, 1054},
    {Structure::sep_event_view, 294, 1, IdPolicy::auto_gen, ReplayScope::range, true, true, 0x2fu, 0, Branch::config, 253, 1129},
    {Structure::sep_event_patch, 990, 4, IdPolicy::strict, ReplayScope::stage, false, true, 0x3bu, 4, Branch::telemetry, 326, 735},
    {Structure::sep_event_legacy, 427, 4, IdPolicy::auto_gen, ReplayScope::none, true, true, 0x37u, 2, Branch::event, 201, 687},
    {Structure::sep_event_v2, 644, 8, IdPolicy::keep_blank, ReplayScope::stage, false, true, 0x25u, 0, Branch::config, 322, 1000},
    {Structure::fts_status, 117, 5, IdPolicy::strict, ReplayScope::stage, true, false, 0x34u, 0, Branch::config, 22, 348},
    {Structure::fts_statuses, 534, 4, IdPolicy::keep_blank, ReplayScope::range, false, true, 0x35u, 4, Branch::config, 328, 608},
    {Structure::fts_status_view, 404, 4, IdPolicy::keep_blank, ReplayScope::stage, true, true, 0x22u, 2, Branch::command, 200, 824},
    {Structure::fts_status_patch, 127, 4, IdPolicy::strict, ReplayScope::none, true, false, 0x14u, 0, Branch::command, 223, 425},
    {Structure::fts_status_legacy, 190, 5, IdPolicy::auto_gen, ReplayScope::stage, true, false, 0x1fu, 5, Branch::config, 367, 475},
    {Structure::fts_status_v2, 905, 5, IdPolicy::keep_blank, ReplayScope::vehicle, false, true, 0x1fu, 4, Branch::command, 147, 684},
    {Structure::payload_frame, 222, 5, IdPolicy::keep_blank, ReplayScope::stage, true, false, 0x3eu, 4, Branch::command, 187, 444},
    {Structure::payload_frames, 952, 2, IdPolicy::auto_gen, ReplayScope::range, false, false, 0x04u, 5, Branch::telemetry, 202, 747},
    {Structure::payload_frame_view, 662, 4, IdPolicy::keep_blank, ReplayScope::stage, false, false, 0x1fu, 4, Branch::config, 143, 1140},
    {Structure::payload_frame_patch, 869, 2, IdPolicy::strict, ReplayScope::range, false, false, 0x21u, 3, Branch::command, 384, 1040},
    {Structure::payload_frame_legacy, 242, 5, IdPolicy::keep_blank, ReplayScope::vehicle, true, true, 0x27u, 1, Branch::config, 202, 630},
    {Structure::payload_frame_v2, 925, 6, IdPolicy::auto_gen, ReplayScope::none, false, false, 0x3eu, 3, Branch::config, 36, 463},
    {Structure::link_quality, 520, 7, IdPolicy::auto_gen, ReplayScope::none, true, true, 0x1fu, 1, Branch::command, 362, 366},
    {Structure::link_qualities, 156, 9, IdPolicy::auto_gen, ReplayScope::vehicle, false, true, 0x06u, 2, Branch::command, 471, 791},
    {Structure::link_quality_view, 826, 8, IdPolicy::auto_gen, ReplayScope::none, false, false, 0x33u, 3, Branch::telemetry, 368, 716},
    {Structure::link_quality_patch, 156, 9, IdPolicy::keep_blank, ReplayScope::stage, true, false, 0x06u, 2, Branch::config, 91, 444},
    {Structure::link_quality_legacy, 145, 5, IdPolicy::auto_gen, ReplayScope::none, false, false, 0x2eu, 2, Branch::config, 169, 658},
    {Structure::link_quality_v2, 923, 2, IdPolicy::strict, ReplayScope::stage, false, false, 0x3bu, 1, Branch::config, 421, 540},
    {Structure::thermal_sample, 454, 7, IdPolicy::strict, ReplayScope::none, true, false, 0x28u, 3, Branch::telemetry, 78, 549},
    {Structure::thermal_samples, 495, 1, IdPolicy::keep_blank, ReplayScope::range, true, true, 0x22u, 5, Branch::command, 214, 613},
    {Structure::thermal_sample_view, 716, 3, IdPolicy::strict, ReplayScope::vehicle, false, false, 0x3bu, 1, Branch::command, 389, 950},
    {Structure::thermal_sample_patch, 821, 5, IdPolicy::strict, ReplayScope::vehicle, false, true, 0x08u, 0, Branch::command, 100, 650},
    {Structure::thermal_sample_legacy, 520, 8, IdPolicy::strict, ReplayScope::stage, true, true, 0x02u, 2, Branch::telemetry, 240, 411},
    {Structure::thermal_sample_v2, 486, 9, IdPolicy::auto_gen, ReplayScope::none, false, false, 0x30u, 2, Branch::event, 149, 186},
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
