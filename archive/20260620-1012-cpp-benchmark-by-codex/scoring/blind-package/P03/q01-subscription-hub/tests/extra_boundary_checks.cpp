// Comprehensive boundary checks for SubscriptionHub.
// Covers: temp string lifetime, re-entrant subscribe/unsubscribe/publish/drain,
// batch-boundary semantics, cancel/new-during-callback, exception isolation,
// uint32_t wrap-around expiry, multi-threaded publish/drain.
#include "subscription_hub.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            ++failures;                                                   \
            std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond); \
        }                                                                 \
    } while (0)

// 1. Temp string inputs: views must outlive the temporary.
static void test_temp_string_lifetime() {
    SubscriptionHub hub;
    std::vector<std::string> got;
    {
        std::string tmp_topic = "orders";
        std::string tmp_payload = "created";
        hub.subscribe(tmp_topic, [&got](const SubscriptionHub::Event& e) {
            got.emplace_back(e.payload);
        }, 0);
    }
    {
        std::string tmp_topic = "orders";
        std::string tmp_payload = "shipped";
        hub.publish(tmp_topic, tmp_payload);
    }
    auto stats = hub.drain(5);
    CHECK(stats.delivered == 1);
    CHECK((got == std::vector<std::string>{"shipped"}));
}

// 2. drain only processes events present at drain start; events published
//    during a callback go to the next drain.
static void test_batch_boundary() {
    SubscriptionHub hub;
    std::vector<std::string> got;
    bool first = true;
    hub.subscribe("t", [&](const SubscriptionHub::Event& e) {
        got.emplace_back(e.payload);
        if (first) {
            first = false;
            hub.publish("t", "second");  // must NOT be delivered this drain
        }
    }, 0);
    hub.publish("t", "first");
    auto s1 = hub.drain(1);
    CHECK(s1.delivered == 1);
    CHECK((got == std::vector<std::string>{"first"}));
    CHECK(hub.pending_events() == 1);
    auto s2 = hub.drain(2);
    CHECK(s2.delivered == 1);
    CHECK((got == (std::vector<std::string>{"first", "second"})));
}

// 3. New subscription during callback does not receive current-batch events.
static void test_new_subscription_in_callback() {
    SubscriptionHub hub;
    std::vector<std::string> a, b;
    SubscriptionHub::SubscriptionId id_b = 0;
    hub.subscribe("t", [&](const SubscriptionHub::Event& e) {
        a.emplace_back(e.payload);
        id_b = hub.subscribe("t", [&b](const SubscriptionHub::Event& e) {
            b.emplace_back(e.payload);
        }, 0);
    }, 0);
    hub.publish("t", "x");
    hub.drain(1);
    CHECK((a == std::vector<std::string>{"x"}));
    CHECK(b.empty());  // new sub created during delivery misses the batch
    CHECK(id_b != 0);
}

// 4. Unsubscribe during callback: this event still delivered (subscribed at
//    delivery start), but not future events.
static void test_cancel_in_callback() {
    SubscriptionHub hub;
    std::vector<std::string> got;
    SubscriptionHub::SubscriptionId self = 0;
    self = hub.subscribe("t", [&](const SubscriptionHub::Event& e) {
        got.emplace_back(e.payload);
        hub.unsubscribe(self);
    }, 0);
    hub.publish("t", "a");
    hub.publish("t", "b");
    auto s = hub.drain(1);
    CHECK(s.delivered == 2);   // both events delivered to the (subscribed) self
    CHECK((got == std::vector<std::string>{"a", "b"}));
    auto s2 = hub.drain(2);    // nothing left, and self is gone
    CHECK(s2.delivered == 0);
}

// 5. Exception in one callback does not stop others; delivered counts all
//    attempts; callback_errors counts throws.
static void test_exception_isolation() {
    SubscriptionHub hub;
    std::vector<std::string> got;
    hub.subscribe("t", [](const SubscriptionHub::Event&) { throw std::runtime_error("boom"); }, 0);
    hub.subscribe("t", [&got](const SubscriptionHub::Event& e) { got.emplace_back(e.payload); }, 0);
    hub.publish("t", "p");
    auto s = hub.drain(1);
    CHECK(s.delivered == 2);
    CHECK(s.callback_errors == 1);
    CHECK((got == std::vector<std::string>{"p"}));
}

// 6. uint32_t wrap-around expiry, TTL boundary (elapsed >= ttl), ttl=0.
static void test_wrap_around_expiry() {
    SubscriptionHub hub;
    auto id = hub.subscribe("t", [](const SubscriptionHub::Event&) {}, 0xFFFFFFF0u);
    // elapsed = 0xFFFFFFFF - 0xFFFFFFF0 = 15 with ttl=15 -> expired
    CHECK(hub.expire_idle(0xFFFFFFFFu, 15u) == 1);
    (void)id;

    // ttl=0 expires immediately even on same tick.
    auto id2 = hub.subscribe("t2", [](const SubscriptionHub::Event&) {}, 100u);
    CHECK(hub.expire_idle(100u, 0u) == 1);
    (void)id2;

    // Just under ttl: not expired.
    auto id3 = hub.subscribe("t3", [](const SubscriptionHub::Event&) {}, 10u);
    CHECK(hub.expire_idle(20u, 11u) == 0);
    // active tick updated to 20 after... no, only callbacks update. So 20-10=10 <11 not expired. ok.
    hub.unsubscribe(id3);
}

// 7. callable destructor re-entry (the public check case), but driven from a
//    drain so we confirm delivery + outside-lock destruction.
static void test_callable_destroy_reentry() {
    SubscriptionHub hub;
    struct Ctrl {
        SubscriptionHub* hub;
        SubscriptionHub::SubscriptionId self = 0;
        std::atomic<bool> armed{false};
        std::atomic<bool> ran{false};
    };
    Ctrl ctrl{&hub};
    struct Reenter {
        Ctrl* c;
        void operator()(const SubscriptionHub::Event&) const { c->ran.store(true); }
        ~Reenter() {
            if (c->armed.load(std::memory_order_relaxed)) {
                c->hub->unsubscribe(c->self);  // re-enter from destructor
            }
        }
    };
    ctrl.self = hub.subscribe("t", Reenter{&ctrl}, 0);
    hub.publish("t", "p");
    auto s = hub.drain(1);  // callback runs (and returns) before destruction
    CHECK(s.delivered == 1);
    CHECK(ctrl.ran.load());
    ctrl.armed.store(true);
    hub.unsubscribe(ctrl.self);  // final owner destroyed outside lock, re-enters
}

// 8. Multithreaded: producers publish, a consumer drains concurrently.
static void test_multithreaded() {
    SubscriptionHub hub;
    std::atomic<std::size_t> delivered{0};
    hub.subscribe("t", [&delivered](const SubscriptionHub::Event&) { delivered.fetch_add(1); }, 0);
    std::vector<std::thread> ts;
    std::atomic<bool> stop{false};
    // producers
    for (int i = 0; i < 4; ++i) {
        ts.emplace_back([&hub, &stop] {
            int n = 0;
            while (!stop.load()) {
                hub.publish("t", std::to_string(n++));
            }
        });
    }
    // drainers
    std::atomic<std::size_t> drained_batches{0};
    for (int i = 0; i < 2; ++i) {
        ts.emplace_back([&hub, &drained_batches] {
            for (int k = 0; k < 200; ++k) {
                hub.drain(k);
                drained_batches.fetch_add(1);
            }
        });
    }
    // wait for drainers to finish their fixed work
    while (drained_batches.load() < 2 * 200) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    stop.store(true);
    for (auto& t : ts) t.join();
    // final drain
    hub.drain(9999);
    // No assertion on exact count, but must not crash/deadlock; delivered > 0.
    CHECK(delivered.load() > 0);
}

int main() {
    test_temp_string_lifetime();
    test_batch_boundary();
    test_new_subscription_in_callback();
    test_cancel_in_callback();
    test_exception_isolation();
    test_wrap_around_expiry();
    test_callable_destroy_reentry();
    test_multithreaded();
    if (failures == 0) {
        std::printf("ALL PASS\n");
        return 0;
    }
    std::printf("%d FAILURES\n", failures);
    return 1;
}
