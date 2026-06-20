#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

enum class SettlementKind {
    authorize,
    capture,
    void_authorization,
    refund,
};

enum class SettlementCode {
    applied,
    duplicate,
    invalid,
    revision_gap,
    insufficient_authorization,
    refund_exceeds_capture,
    already_voided,
};

struct SettlementCommand {
    std::string_view merchant_id;
    std::string_view order_id;
    std::string_view message_id;
    SettlementKind kind = SettlementKind::authorize;
    std::uint64_t revision = 0;
    std::int64_t amount_minor = 0;
};

struct SettlementReply {
    SettlementCode code = SettlementCode::invalid;
    std::uint64_t accepted_revision = 0;
    std::int64_t authorized_minor = 0;
    std::int64_t captured_minor = 0;
    std::int64_t refunded_minor = 0;
};

struct SettlementSnapshot {
    std::string merchant_id;
    std::string order_id;
    std::uint64_t accepted_revision = 0;
    std::int64_t authorized_minor = 0;
    std::int64_t captured_minor = 0;
    std::int64_t refunded_minor = 0;
    bool voided = false;
};

struct AuditRecord {
    std::string merchant_id;
    std::string order_id;
    std::string message_id;
    SettlementKind kind = SettlementKind::authorize;
    std::uint64_t revision = 0;
    std::int64_t amount_minor = 0;
    SettlementReply reply;
};

class SettlementJournal {
public:
    using AuditSink = std::function<void(const AuditRecord&)>;

    SettlementJournal();
    explicit SettlementJournal(AuditSink audit_sink);

    SettlementReply apply(const SettlementCommand& command);
    std::optional<SettlementSnapshot> lookup(std::string_view merchant_id,
                                             std::string_view order_id) const;
    void set_audit_sink(AuditSink audit_sink);
    std::size_t accepted_count() const;

private:
    struct State;
    std::shared_ptr<State> state_;
};
