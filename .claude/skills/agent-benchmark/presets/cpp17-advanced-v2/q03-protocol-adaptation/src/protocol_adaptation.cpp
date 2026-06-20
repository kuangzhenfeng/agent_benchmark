#include "protocol_adaptation.hpp"

#include <array>

namespace {

constexpr std::array<StructureRule, 24> kRules{{
    {StructureId::settlement_record, 101, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_records, 102, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_record_view, 103, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_record_patch, 104, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_receipt, 105, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_receipts, 106, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_receipt_view, 107, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_receipt_patch, 108, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_reservation, 109, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_reservations, 110, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_reservation_view, 111, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_reservation_patch, 112, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_reversal, 113, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_reversals, 114, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_reversal_view, 115, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_reversal_patch, 116, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_adjustment, 117, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_adjustments, 118, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_adjustment_view, 119, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_adjustment_patch, 120, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_attachment, 121, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_attachments, 122, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_attachment_view, 123, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
    {StructureId::settlement_attachment_patch, 124, 1, IdentifierPolicy::required, ReplayDomain::account, false, false},
}};

}  // 匿名命名空间

std::optional<StructureRule> rule_for(StructureId id) {
    for (const StructureRule& rule : kRules) {
        if (rule.id == id) {
            return rule;
        }
    }
    return std::nullopt;
}

bool accepts(StructureId id, const IncomingRecord& record) {
    const auto rule = rule_for(id);
    if (!rule.has_value()) {
        return false;
    }
    if (rule->identifier_policy == IdentifierPolicy::required &&
        record.identifier.empty()) {
        return false;
    }
    if (rule->source_time_required && !record.has_source_time) {
        return false;
    }
    if (rule->ordinal_required && !record.has_ordinal) {
        return false;
    }
    return true;
}

bool replayable(StructureId id) {
    const auto rule = rule_for(id);
    return rule.has_value() && rule->replay_domain != ReplayDomain::none;
}
