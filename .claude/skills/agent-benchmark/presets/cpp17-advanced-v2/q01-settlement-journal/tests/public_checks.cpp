#include "settlement_journal.hpp"

#include <cassert>
#include <string>

namespace {

SettlementCommand command(std::string_view merchant, std::string_view order,
                          std::string_view message, SettlementKind kind,
                          std::uint64_t revision, std::int64_t amount) {
    return SettlementCommand{merchant, order, message, kind, revision, amount};
}

}  // 匿名命名空间

int main() {
    SettlementJournal journal;

    const auto authorized = journal.apply(
        command("merchant-a", "order-7", "m-1", SettlementKind::authorize, 1, 100));
    assert(authorized.code == SettlementCode::applied);
    assert(authorized.accepted_revision == 1);

    const auto captured = journal.apply(
        command("merchant-a", "order-7", "m-2", SettlementKind::capture, 2, 70));
    assert(captured.code == SettlementCode::applied);
    assert(captured.captured_minor == 70);

    const auto refused_refund = journal.apply(
        command("merchant-a", "order-7", "m-3", SettlementKind::refund, 3, 71));
    assert(refused_refund.code == SettlementCode::refund_exceeds_capture);
    assert(refused_refund.accepted_revision == 2);

    const auto refunded = journal.apply(
        command("merchant-a", "order-7", "m-3", SettlementKind::refund, 3, 30));
    assert(refunded.code == SettlementCode::applied);
    assert(refunded.refunded_minor == 30);
    assert(journal.accepted_count() == 3);

    const auto replay = journal.apply(
        command("merchant-a", "not-the-original-order", "m-2",
                SettlementKind::void_authorization, 999, 0));
    assert(replay.code == SettlementCode::duplicate);
    assert(replay.accepted_revision == 2);
    assert(replay.captured_minor == 70);

    const auto another_merchant = journal.apply(
        command("merchant-b", "order-7", "m-2", SettlementKind::authorize, 1, 9));
    assert(another_merchant.code == SettlementCode::applied);

    std::string merchant = "merchant-temporary";
    std::string order = "order-temporary";
    std::string message = "message-temporary";
    assert(journal.apply(command(merchant, order, message,
                                 SettlementKind::authorize, 1, 33))
               .code == SettlementCode::applied);
    merchant.assign("xxxxxxxxxxxxxxxxxx");
    order.assign("yyyyyyyyyyyyyyy");
    message.assign("zzzzzzzzzzzzzzzzz");
    const auto snapshot = journal.lookup("merchant-temporary", "order-temporary");
    assert(snapshot.has_value());
    assert(snapshot->authorized_minor == 33);
}
