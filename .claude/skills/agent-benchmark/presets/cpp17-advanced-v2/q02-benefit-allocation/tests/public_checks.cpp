#include "benefit_allocation.hpp"

#include <cassert>
#include <vector>

int main() {
    BenefitAllocator allocator({
        {"spring", 100, 80, 10, 20},
        {"bulk", 100, 80, 0, 100},
    });

    const auto first = allocator.claim({"spring", "alice", "claim-1", 60}, 10);
    assert(first.code == ClaimCode::granted);
    assert(first.campaign_remaining_minor == 40);
    assert(first.customer_used_minor == 60);

    const auto replay = allocator.claim({"spring", "someone-else", "claim-1", 1}, 999);
    assert(replay.code == ClaimCode::duplicate);
    assert(replay.granted_minor == 60);
    assert(replay.campaign_remaining_minor == 40);
    assert(replay.customer_used_minor == 60);

    const auto customer_limited =
        allocator.claim({"spring", "alice", "claim-2", 30}, 11);
    assert(customer_limited.code == ClaimCode::customer_limit);

    const auto outside = allocator.claim({"spring", "bob", "retry-me", 10}, 20);
    assert(outside.code == ClaimCode::outside_window);

    ClaimBatch rejected{
        "batch-1",
        {
            {"bulk", "alice", "batch-claim-1", 70},
            {"bulk", "alice", "batch-claim-2", 20},
        },
    };
    const auto first_batch = allocator.claim_batch(rejected, 1);
    assert(first_batch.code == BatchCode::rejected);
    assert(first_batch.replies.size() == 1);
    assert(first_batch.replies.front().code == ClaimCode::customer_limit);
    assert(first_batch.replies.front().campaign_remaining_minor == 30);
    assert(first_batch.replies.front().customer_used_minor == 70);

    const auto bulk_after_rejection = allocator.view("bulk");
    assert(bulk_after_rejection.has_value());
    assert(bulk_after_rejection->campaign_remaining_minor == 100);
    assert(bulk_after_rejection->granted_claim_count == 0);

    const auto duplicate_batch = allocator.claim_batch(
        {"batch-1", {{"bulk", "bob", "different-claim", 1}}}, 2);
    assert(duplicate_batch.code == BatchCode::duplicate);
    assert(duplicate_batch.replies.size() == 1);
    assert(duplicate_batch.replies.front().code == ClaimCode::customer_limit);
}
