#include "settlement_journal.hpp"

#include <mutex>
#include <utility>
#include <vector>

namespace {

struct Order {
    std::string_view merchant_id;
    std::string_view order_id;
    std::uint64_t revision = 0;
    std::int64_t authorized_minor = 0;
    std::int64_t captured_minor = 0;
    std::int64_t refunded_minor = 0;
    bool voided = false;
};

struct Replay {
    std::string_view merchant_id;
    std::string_view message_id;
    SettlementReply reply;
};

SettlementReply reply_for(SettlementCode code, const Order* order) {
    SettlementReply reply;
    reply.code = code;
    if (order != nullptr) {
        reply.accepted_revision = order->revision;
        reply.authorized_minor = order->authorized_minor;
        reply.captured_minor = order->captured_minor;
        reply.refunded_minor = order->refunded_minor;
    }
    return reply;
}

}  // 匿名命名空间

struct SettlementJournal::State {
    mutable std::mutex mutex;
    std::vector<Order> orders;
    std::vector<Replay> replays;
    AuditSink audit_sink;
};

SettlementJournal::SettlementJournal()
    : state_(std::make_shared<State>()) {}

SettlementJournal::SettlementJournal(AuditSink audit_sink)
    : state_(std::make_shared<State>()) {
    state_->audit_sink = std::move(audit_sink);
}

SettlementReply SettlementJournal::apply(const SettlementCommand& command) {
    std::lock_guard<std::mutex> lock(state_->mutex);

    for (const Replay& replay : state_->replays) {
        if (replay.message_id == command.message_id) {
            SettlementReply reply = replay.reply;
            reply.code = SettlementCode::duplicate;
            return reply;
        }
    }

    if (command.merchant_id.empty() || command.order_id.empty() ||
        command.message_id.empty()) {
        return {};
    }

    Order* order = nullptr;
    for (Order& candidate : state_->orders) {
        if (candidate.merchant_id == command.merchant_id &&
            candidate.order_id == command.order_id) {
            order = &candidate;
            break;
        }
    }

    if (order == nullptr) {
        if (command.kind != SettlementKind::authorize || command.revision != 1 ||
            command.amount_minor <= 0) {
            return {};
        }
        state_->orders.push_back(Order{command.merchant_id, command.order_id, 1,
                                       command.amount_minor, 0, 0, false});
        order = &state_->orders.back();
    } else {
        if (command.revision != order->revision + 1) {
            return reply_for(SettlementCode::revision_gap, order);
        }
        if (order->voided) {
            return reply_for(SettlementCode::already_voided, order);
        }

        switch (command.kind) {
            case SettlementKind::capture:
                if (command.amount_minor <= 0 ||
                    command.amount_minor >
                        order->authorized_minor - order->captured_minor) {
                    return reply_for(SettlementCode::insufficient_authorization,
                                     order);
                }
                order->captured_minor += command.amount_minor;
                break;
            case SettlementKind::refund:
                if (command.amount_minor <= 0 ||
                    command.amount_minor >
                        order->captured_minor - order->refunded_minor) {
                    return reply_for(SettlementCode::refund_exceeds_capture,
                                     order);
                }
                order->refunded_minor += command.amount_minor;
                break;
            case SettlementKind::void_authorization:
                if (command.amount_minor != 0 || order->captured_minor != 0) {
                    return reply_for(SettlementCode::invalid, order);
                }
                order->voided = true;
                break;
            case SettlementKind::authorize:
                return reply_for(SettlementCode::invalid, order);
        }
        ++order->revision;
    }

    const SettlementReply reply = reply_for(SettlementCode::applied, order);
    state_->replays.push_back(
        Replay{command.merchant_id, command.message_id, reply});

    if (state_->audit_sink) {
        state_->audit_sink(AuditRecord{std::string(command.merchant_id),
                                       std::string(command.order_id),
                                       std::string(command.message_id),
                                       command.kind, command.revision,
                                       command.amount_minor, reply});
    }
    return reply;
}

std::optional<SettlementSnapshot> SettlementJournal::lookup(
    std::string_view merchant_id, std::string_view order_id) const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    for (const Order& order : state_->orders) {
        if (order.merchant_id == merchant_id && order.order_id == order_id) {
            return SettlementSnapshot{std::string(order.merchant_id),
                                      std::string(order.order_id), order.revision,
                                      order.authorized_minor, order.captured_minor,
                                      order.refunded_minor, order.voided};
        }
    }
    return std::nullopt;
}

void SettlementJournal::set_audit_sink(AuditSink audit_sink) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->audit_sink = std::move(audit_sink);
}

std::size_t SettlementJournal::accepted_count() const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->replays.size();
}
