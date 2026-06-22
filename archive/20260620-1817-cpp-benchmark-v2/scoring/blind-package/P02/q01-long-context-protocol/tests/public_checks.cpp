#include "range_validator.hpp"

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
    check_one(Structure::stage_state, FieldRule{Structure::stage_state, 576, static_cast<std::uint8_t>(2), IdPolicy::auto_gen, ReplayScope::vehicle, false, true, 1u, 2, Branch::command, 191, 219});
    check_one(Structure::thrust_reports, FieldRule{Structure::thrust_reports, 482, static_cast<std::uint8_t>(2), IdPolicy::keep_blank, ReplayScope::stage, false, false, 47u, 1, Branch::telemetry, 449, 916});
    check_one(Structure::thrust_report_v2, FieldRule{Structure::thrust_report_v2, 986, static_cast<std::uint8_t>(7), IdPolicy::keep_blank, ReplayScope::vehicle, true, true, 4u, 1, Branch::telemetry, 116, 519});
    check_one(Structure::nav_solution_legacy, FieldRule{Structure::nav_solution_legacy, 565, static_cast<std::uint8_t>(8), IdPolicy::keep_blank, ReplayScope::range, false, false, 16u, 6, Branch::event, 414, 872});
    check_one(Structure::sep_event_v2, FieldRule{Structure::sep_event_v2, 552, static_cast<std::uint8_t>(6), IdPolicy::strict, ReplayScope::vehicle, false, false, 31u, 3, Branch::telemetry, 42, 259});
    check_one(Structure::payload_frame, FieldRule{Structure::payload_frame, 810, static_cast<std::uint8_t>(7), IdPolicy::keep_blank, ReplayScope::vehicle, true, false, 51u, 5, Branch::telemetry, 142, 766});
    check_one(Structure::link_qualities, FieldRule{Structure::link_qualities, 761, static_cast<std::uint8_t>(9), IdPolicy::keep_blank, ReplayScope::range, false, false, 51u, 5, Branch::command, 81, 926});
    check_one(Structure::thermal_sample_patch, FieldRule{Structure::thermal_sample_patch, 197, static_cast<std::uint8_t>(5), IdPolicy::strict, ReplayScope::vehicle, true, true, 6u, 0, Branch::command, 324, 475});
    // flag_active 抽样: stage_state
    assert(flag_active(Structure::stage_state, ModeBit::arm) == true);
    assert(flag_active(Structure::stage_state, ModeBit::ignite) == false);
    assert(flag_active(Structure::stage_state, ModeBit::hold) == false);
    assert(flag_active(Structure::stage_state, ModeBit::safe) == false);
    assert(flag_active(Structure::stage_state, ModeBit::redundant) == false);
    assert(flag_active(Structure::stage_state, ModeBit::debug) == false);
    return 0;
}
