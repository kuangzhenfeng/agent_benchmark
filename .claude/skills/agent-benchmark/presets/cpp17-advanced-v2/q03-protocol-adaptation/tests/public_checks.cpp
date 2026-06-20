#include "protocol_adaptation.hpp"

#include <cassert>
#include <cstddef>

namespace {

struct Expected {
    StructureId id;
    std::uint16_t wire_tag;
    std::uint16_t revision;
    IdentifierPolicy identifier_policy;
    ReplayDomain replay_domain;
    bool source_time_required;
    bool ordinal_required;
};

void assert_rule(const Expected& expected) {
    const auto actual = rule_for(expected.id);
    assert(actual.has_value());
    assert(actual->wire_tag == expected.wire_tag);
    assert(actual->revision == expected.revision);
    assert(actual->identifier_policy == expected.identifier_policy);
    assert(actual->replay_domain == expected.replay_domain);
    assert(actual->source_time_required == expected.source_time_required);
    assert(actual->ordinal_required == expected.ordinal_required);
    const IncomingRecord missing_all{"", "令牌", false, false};
    const bool should_accept_empty =
        expected.identifier_policy != IdentifierPolicy::required &&
        !expected.source_time_required && !expected.ordinal_required;
    assert(accepts(expected.id, missing_all) == should_accept_empty);
    const IncomingRecord complete{"标识", "令牌", true, true};
    assert(accepts(expected.id, complete));
    assert(replayable(expected.id) ==
           (expected.replay_domain != ReplayDomain::none));
}

}  // 匿名命名空间

int main() {
    assert_rule({StructureId::settlement_record, 601, 2, IdentifierPolicy::preserved_if_empty, ReplayDomain::account, true, true});
    assert_rule({StructureId::settlement_records, 618, 4, IdentifierPolicy::required, ReplayDomain::none, false, true});
    assert_rule({StructureId::settlement_record_patch, 652, 3, IdentifierPolicy::preserved_if_empty, ReplayDomain::merchant, true, false});
    assert_rule({StructureId::settlement_receipt_view, 703, 4, IdentifierPolicy::preserved_if_empty, ReplayDomain::batch, true, true});
    assert_rule({StructureId::settlement_reservation_patch, 788, 4, IdentifierPolicy::generated_if_empty, ReplayDomain::merchant, false, false});
    assert_rule({StructureId::settlement_reversal_view, 839, 5, IdentifierPolicy::generated_if_empty, ReplayDomain::batch, true, false});
    assert_rule({StructureId::settlement_adjustment_view, 624, 3, IdentifierPolicy::preserved_if_empty, ReplayDomain::batch, true, true});
    assert_rule({StructureId::settlement_attachment_patch, 709, 3, IdentifierPolicy::generated_if_empty, ReplayDomain::merchant, false, false});
}
