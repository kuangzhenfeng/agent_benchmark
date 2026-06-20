#include "settlement_journal.hpp"

#include <cassert>

int main() {
    SettlementJournal journal;
    std::size_t audits = 0;
    journal.set_audit_sink([&](const AuditRecord&) {
        ++audits;
        assert(journal.accepted_count() == 1);
        const auto snapshot = journal.lookup("merchant", "order");
        assert(snapshot.has_value());
        throw 7;
    });

    const auto result = journal.apply(
        {"merchant", "order", "message", SettlementKind::authorize, 1, 20});
    assert(result.code == SettlementCode::applied);
    assert(audits == 1);
    assert(journal.accepted_count() == 1);
}
