#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

enum class StructureId {
    settlement_record,
    settlement_records,
    settlement_record_view,
    settlement_record_patch,
    settlement_receipt,
    settlement_receipts,
    settlement_receipt_view,
    settlement_receipt_patch,
    settlement_reservation,
    settlement_reservations,
    settlement_reservation_view,
    settlement_reservation_patch,
    settlement_reversal,
    settlement_reversals,
    settlement_reversal_view,
    settlement_reversal_patch,
    settlement_adjustment,
    settlement_adjustments,
    settlement_adjustment_view,
    settlement_adjustment_patch,
    settlement_attachment,
    settlement_attachments,
    settlement_attachment_view,
    settlement_attachment_patch,
};

enum class IdentifierPolicy {
    required,
    preserved_if_empty,
    generated_if_empty,
};

enum class ReplayDomain {
    none,
    account,
    merchant,
    batch,
};

struct StructureRule {
    StructureId id;
    std::uint16_t wire_tag;
    std::uint16_t revision;
    IdentifierPolicy identifier_policy;
    ReplayDomain replay_domain;
    bool source_time_required;
    bool ordinal_required;
};

struct IncomingRecord {
    std::string_view identifier;
    std::string_view replay_token;
    bool has_source_time = false;
    bool has_ordinal = false;
};

std::optional<StructureRule> rule_for(StructureId id);
bool accepts(StructureId id, const IncomingRecord& record);
bool replayable(StructureId id);
