#include "benefit_allocation.hpp"

#include <cassert>
#include <string>

int main() {
    BenefitAllocator allocator({{"window", 30, 30, 5, 10}});
    std::size_t audits = 0;
    allocator.set_audit_sink([&](const BenefitAuditRecord&) {
        ++audits;
        const auto state = allocator.view("window");
        assert(state.has_value());
        assert(state->campaign_remaining_minor == 20);
        throw 5;
    });

    const auto outside = allocator.claim({"window", "customer", "retry", 10}, 4);
    assert(outside.code == ClaimCode::outside_window);
    assert(audits == 0);

    std::string campaign = "window";
    std::string customer = "customer";
    std::string claim = "retry";
    const auto accepted = allocator.claim({campaign, customer, claim, 10}, 5);
    assert(accepted.code == ClaimCode::granted);
    assert(audits == 1);
    campaign = "xxxxxx";
    customer = "yyyyyyyy";
    claim = "zzzzz";
    const auto replay = allocator.claim({"window", "other", "retry", 999}, 99);
    assert(replay.code == ClaimCode::duplicate);
    assert(replay.granted_minor == 10);
}
