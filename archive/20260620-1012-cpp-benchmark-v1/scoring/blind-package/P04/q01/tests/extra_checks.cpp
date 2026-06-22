#include "subscription_hub.hpp"

#include <atomic>
#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void test_temporary_string_ownership() {
    SubscriptionHub hub;
    std::vector<std::string> received;
    hub.subscribe(std::string("orders"), [&received](const SubscriptionHub::Event& ev) {
        received.emplace_back(ev.payload);
    }, 0);

    // Publish from temporary std::string; the view must outlive the call.
    hub.publish(std::string("orders"), std::string("payload-").append("xyz"));
    const auto stats = hub.drain(1);
    assert(stats.delivered == 1);
    assert((received == std::vector<std::string>{"payload-xyz"}));
}

void test_batch_boundary() {
    SubscriptionHub hub;

    std::vector<std::uint64_t> seen_seqs;
    hub.subscribe("topic", [&](const SubscriptionHub::Event& ev) {
        seen_seqs.push_back(ev.sequence);
        if (ev.sequence == 1) {
            // Re-publish during the callback: must NOT be processed this drain.
            hub.publish("topic", "second");
        }
    }, 0);

    hub.publish("topic", "first");  // sequence 1
    const auto stats = hub.drain(10);
    // Only the original event should be delivered this drain.
    assert(stats.delivered == 1);
    assert((seen_seqs == std::vector<std::uint64_t>{1}));
    assert(hub.pending_events() == 1);  // second remains for next drain

    const auto stats2 = hub.drain(11);
    assert(stats2.delivered == 1);
    assert(hub.pending_events() == 0);
}

void test_exception_isolation() {
    SubscriptionHub hub;
    std::vector<std::string> received;
    hub.subscribe("topic", [&](const SubscriptionHub::Event&) {
        throw std::runtime_error("boom");
    }, 0);
    hub.subscribe("topic", [&](const SubscriptionHub::Event& ev) {
        received.emplace_back(ev.payload);
    }, 0);
    hub.subscribe("topic", [&](const SubscriptionHub::Event& ev) {
        received.emplace_back(ev.payload);
    }, 0);

    hub.publish("topic", "data");
    const auto stats = hub.drain(5);
    assert(stats.delivered == 3);              // all three attempts
    assert(stats.callback_errors == 1);        // one threw
    assert(received.size() == 2);              // the other two still ran
    assert(hub.pending_events() == 0);         // event consumed, not re-delivered
}

struct UnsubControl {
    SubscriptionHub* hub;
    SubscriptionHub::SubscriptionId self_id = 0;
    SubscriptionHub::SubscriptionId target_id = 0;
    std::vector<std::uint64_t>* target_log = nullptr;
};

void test_unsubscribe_during_callback_v2() {
    SubscriptionHub hub;
    std::vector<std::uint64_t> a_log;
    std::vector<std::uint64_t> b_log;

    UnsubControl control{&hub, 0, 0, &b_log};
    auto a_id = hub.subscribe("topic", [&control, &a_log](const SubscriptionHub::Event& ev) {
        a_log.push_back(ev.sequence);
        if (ev.sequence == 1 && control.target_id != 0) {
            control.hub->unsubscribe(control.target_id);
        }
    }, 0);
    control.self_id = a_id;

    auto b_id = hub.subscribe("topic", [&b_log](const SubscriptionHub::Event& ev) {
        b_log.push_back(ev.sequence);
    }, 0);
    control.target_id = b_id;

    hub.publish("topic", "e1");  // seq 1
    hub.publish("topic", "e2");  // seq 2
    const auto stats = hub.drain(7);
    (void)stats;

    // a gets both events. b is unsubscribed during e1's dispatch (from a's
    // callback); b may or may not receive e1 depending on iteration order, but
    // must not receive e2.
    assert(a_log.size() == 2);
    // b should not have received event 2.
    bool b_saw_e2 = false;
    for (auto s : b_log) if (s == 2) b_saw_e2 = true;
    assert(!b_saw_e2);
}

void test_new_subscription_during_callback_skips_batch() {
    SubscriptionHub hub;
    std::vector<std::uint64_t> existing_log;
    std::vector<std::uint64_t> new_log;

    hub.subscribe("topic", [&](const SubscriptionHub::Event& ev) {
        existing_log.push_back(ev.sequence);
        if (ev.sequence == 1) {
            // Subscribe a new callback mid-drain: it must NOT receive the
            // already-snapshotted events.
            hub.subscribe("topic", [&new_log](const SubscriptionHub::Event& e) {
                new_log.push_back(e.sequence);
            }, 7);
        }
    }, 0);

    hub.publish("topic", "e1");  // seq 1
    hub.publish("topic", "e2");  // seq 2
    hub.drain(7);
    assert(existing_log.size() == 2);
    assert(new_log.empty());  // new subscriber missed this batch

    // New subscriber receives future events.
    hub.publish("topic", "e3");
    hub.drain(8);
    assert(new_log.size() == 1);
}

void test_ttl_zero_expires_immediately() {
    SubscriptionHub hub;
    hub.subscribe("t", [](const SubscriptionHub::Event&) {}, 100);
    const auto expired = hub.expire_idle(100, 0);
    assert(expired == 1);
}

void test_wraparound_expiry() {
    SubscriptionHub hub;
    // Subscribe near the top of the uint32 range.
    hub.subscribe("t", [](const SubscriptionHub::Event&) {}, 0xFFFFFFFE);
    // now_tick wraps past 0; elapsed = 4 - 0xFFFFFFFE = 6 (mod 2^32).
    // ttl = 5 -> 6 >= 5 -> expired.
    const auto expired = hub.expire_idle(4, 5);
    assert(expired == 1);

    // Now subscribe at 0xFFFFFFFE again, ttl = 10. elapsed 6 < 10 -> survives.
    hub.subscribe("t", [](const SubscriptionHub::Event&) {}, 0xFFFFFFFE);
    const auto expired2 = hub.expire_idle(4, 10);
    assert(expired2 == 0);
}

void test_recursive_drain() {
    SubscriptionHub hub;
    std::vector<std::uint64_t> outer_log;
    std::vector<std::uint64_t> inner_log;

    hub.subscribe("topic", [&](const SubscriptionHub::Event& ev) {
        outer_log.push_back(ev.sequence);
        if (ev.sequence == 1) {
            // Re-enter drain from within the callback. The inner drain runs
            // against whatever is currently pending (nothing, or new events).
            hub.drain(20);
        }
    }, 0);

    hub.publish("topic", "e1");  // seq 1
    hub.publish("topic", "e2");  // seq 2
    const auto stats = hub.drain(10);
    // Both events delivered by the outer drain; inner drain saw nothing.
    assert(stats.delivered == 2);
    assert(outer_log.size() == 2);
}

}  // namespace

int main() {
    test_temporary_string_ownership();
    test_batch_boundary();
    test_exception_isolation();
    test_unsubscribe_during_callback_v2();
    test_new_subscription_during_callback_skips_batch();
    test_ttl_zero_expires_immediately();
    test_wraparound_expiry();
    test_recursive_drain();
    return 0;
}
