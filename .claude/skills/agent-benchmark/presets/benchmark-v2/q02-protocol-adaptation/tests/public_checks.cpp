#include "protocol_adapter.hpp"

#include <cassert>
#include <cstdint>

namespace {

void check_one(Structure id, const FieldRule& exp) {
    const auto r = rule_for(id);
    assert(r.has_value());
    assert(r->route_code == exp.route_code);
    assert(r->schema_rev == exp.schema_rev);
    assert(r->id_policy == exp.id_policy);
    assert(r->replay_scope == exp.replay_scope);
    assert(r->stamp_required == exp.stamp_required);
    assert(r->ordinal_required == exp.ordinal_required);
    assert(r->mode_flag == exp.mode_flag);
    assert(r->phase_value == exp.phase_value);
    assert(r->branch == exp.branch);
    assert(r->band_min == exp.band_min);
    assert(r->band_max == exp.band_max);
    // 派生语义
    assert(replayable(id) == (exp.replay_scope != ReplayScope::none));
    assert(phase_rank(id) == exp.phase_value);
    assert(branch_of(id) == exp.branch);
    assert(within_band(id, exp.band_min));
    assert(within_band(id, exp.band_max));
    assert(!within_band(id, exp.band_min - 1));
    assert(!within_band(id, exp.band_max + 1));
}

}  // 匿名命名空间

int main() {
    check_one(Structure::stage_state, FieldRule{Structure::stage_state, 765, static_cast<std::uint8_t>(5), IdPolicy::keep_blank, ReplayScope::vehicle, true, true, 11u, 3, Branch::config, 313, 925});
    check_one(Structure::thrust_reports, FieldRule{Structure::thrust_reports, 521, static_cast<std::uint8_t>(2), IdPolicy::strict, ReplayScope::none, true, false, 48u, 0, Branch::event, 202, 452});
    check_one(Structure::thrust_report_v2, FieldRule{Structure::thrust_report_v2, 453, static_cast<std::uint8_t>(2), IdPolicy::keep_blank, ReplayScope::vehicle, true, false, 51u, 5, Branch::config, 89, 811});
    check_one(Structure::nav_solution_legacy, FieldRule{Structure::nav_solution_legacy, 617, static_cast<std::uint8_t>(2), IdPolicy::keep_blank, ReplayScope::none, true, true, 54u, 1, Branch::command, 241, 796});
    check_one(Structure::sep_event_v2, FieldRule{Structure::sep_event_v2, 644, static_cast<std::uint8_t>(6), IdPolicy::strict, ReplayScope::stage, true, false, 43u, 1, Branch::event, 482, 1430});
    check_one(Structure::payload_frame, FieldRule{Structure::payload_frame, 108, static_cast<std::uint8_t>(5), IdPolicy::auto_gen, ReplayScope::vehicle, false, true, 45u, 5, Branch::telemetry, 363, 994});
    check_one(Structure::link_qualities, FieldRule{Structure::link_qualities, 683, static_cast<std::uint8_t>(6), IdPolicy::keep_blank, ReplayScope::vehicle, false, false, 1u, 2, Branch::config, 471, 791});
    check_one(Structure::thermal_sample_patch, FieldRule{Structure::thermal_sample_patch, 821, static_cast<std::uint8_t>(7), IdPolicy::auto_gen, ReplayScope::none, true, true, 2u, 3, Branch::telemetry, 447, 1033});
    // flag_active 抽样: stage_state mode_flag=0x0b
    assert(flag_active(Structure::stage_state, ModeBit::arm) == true);
    assert(flag_active(Structure::stage_state, ModeBit::ignite) == true);
    assert(flag_active(Structure::stage_state, ModeBit::hold) == false);
    assert(flag_active(Structure::stage_state, ModeBit::safe) == true);
    assert(flag_active(Structure::stage_state, ModeBit::redundant) == false);
    assert(flag_active(Structure::stage_state, ModeBit::debug) == false);
    return 0;
}
