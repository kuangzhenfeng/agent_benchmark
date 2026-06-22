// Extra boundary checks for RoutingConfig.
#include "routing_config.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <memory>
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

// 1. Validation: empty prefix / empty target / duplicate prefix -> false, no change.
static void test_validation() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    CHECK(config.current().version == 0);
    CHECK(!config.reload(Config{0, {{"", "a"}}}));       // empty prefix
    CHECK(!config.reload(Config{0, {{"/", ""}}}));       // empty target
    CHECK(!config.reload(Config{0, {{"/", "a"}, {"/", "b"}}})); // dup prefix
    CHECK(config.current().version == 0);                // unchanged
}

// 2. Version strictly +1 on success; snapshot immutable & self-consistent.
static void test_version_and_snapshot() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    CHECK(config.reload(Config{0, {{"/", "a"}, {"/v1", "b"}}}));
    auto snap1 = config.snapshot();
    CHECK(snap1->version == 1);
    CHECK(config.reload(Config{0, {{"/", "c"}}}));
    auto snap2 = config.snapshot();
    CHECK(snap2->version == 2);
    CHECK(snap1->version == 1);     // old snapshot still intact (not mutated)
    CHECK(snap1->routes.size() == 2);
}

// 3. find_target longest-prefix match, no match -> "".
static void test_find_target() {
    RoutingConfig config(Config{0, {{"/", "root"}, {"/v1", "v1"}, {"/v1/users", "users"}}});
    CHECK(config.find_target("/v1/users/123") == "users");
    CHECK(config.find_target("/v1/items") == "v1");
    CHECK(config.find_target("/health") == "root");
    CHECK(config.find_target("nomatch").empty());
    CHECK(config.find_target("").empty());
}

// 4. Observer notified once with the new snapshot; callbacks created after
//    reload do not see past; observer sees just-published immutable snapshot.
static void test_observer_notify() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    std::vector<std::shared_ptr<const Config>> seen;
    RoutingConfig::ObserverId id = config.subscribe([&](std::shared_ptr<const Config> s) {
        seen.push_back(s);
    });
    CHECK(config.reload(Config{0, {{"/", "b"}}}));
    CHECK(seen.size() == 1);
    CHECK(seen[0]->version == 1);
    config.unsubscribe(id);
    CHECK(config.reload(Config{0, {{"/", "c"}}}));
    CHECK(seen.size() == 1);  // unsubscribed: not notified again
}

// 5. Observer exception does not stop others; observer_error_count increments.
static void test_observer_exception_isolation() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    std::atomic<int> hits{0};
    config.subscribe([](std::shared_ptr<const Config>) { throw std::runtime_error("x"); });
    config.subscribe([&](std::shared_ptr<const Config>) { hits.fetch_add(1); });
    CHECK(config.reload(Config{0, {{"/", "b"}}}));
    CHECK(hits.load() == 1);                       // second still called
    CHECK(config.observer_error_count() == 1);
}

// 6. Re-entrant observer: subscribe/unsubscribe/reload/snapshot inside callback.
static void test_observer_reentrancy() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    std::atomic<int> self_calls{0};
    RoutingConfig::ObserverId self = 0;
    self = config.subscribe([&](std::shared_ptr<const Config> s) {
        self_calls.fetch_add(1);
        // re-enter safely
        auto snap = config.snapshot();
        (void)snap;
        (void)s;
        if (self_calls.load() == 1) {
            config.unsubscribe(self);  // affects FUTURE reload only
        }
    });
    CHECK(config.reload(Config{0, {{"/", "b"}}}));   // notifies self (1)
    CHECK(self_calls.load() == 1);
    CHECK(config.reload(Config{0, {{"/", "c"}}}));   // self already unsubscribed
    CHECK(self_calls.load() == 1);
}

// 7. New subscription inside observer callback does NOT receive current notice.
static void test_new_observer_in_callback() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    std::atomic<int> late_calls{0};
    RoutingConfig::ObserverId late_id = 0;
    config.subscribe([&](std::shared_ptr<const Config>) {
        if (late_id == 0) {
            late_id = config.subscribe([&](std::shared_ptr<const Config>) {
                late_calls.fetch_add(1);
            });
        }
    });
    CHECK(config.reload(Config{0, {{"/", "b"}}}));   // late not yet subscribed
    CHECK(late_calls.load() == 0);
    CHECK(config.reload(Config{0, {{"/", "c"}}}));   // late now subscribed -> notified
    CHECK(late_calls.load() == 1);
}

// 8. Observer callable destructor re-entry: destroying the stored Observer
//    (on unsubscribe) re-enters the config — must not deadlock, must be outside lock.
static void test_observer_destroy_reentry() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    struct Ctrl {
        RoutingConfig* config;
        RoutingConfig::ObserverId self = 0;
        bool armed = false;
    };
    Ctrl ctrl{&config};
    struct Reenter {
        Ctrl* c;
        Reenter(const Reenter&) = default;
        void operator()(std::shared_ptr<const Config>) const {}
        ~Reenter() {
            if (c->armed) {
                c->config->unsubscribe(c->self);  // re-enter from destructor
            }
        }
    };
    ctrl.self = config.subscribe(Reenter{&ctrl});
    ctrl.armed = true;
    config.unsubscribe(ctrl.self);  // final owner destroyed outside lock, re-enters
}

// 9. Cross-instance current(): pin for `first` survives current() on `second`
//    and a reload on `first`.
static void test_cross_instance_current() {
    RoutingConfig first(Config{0, {{"/", "first"}}});
    RoutingConfig second(Config{0, {{"/", "second"}}});
    const Config& first_v0 = first.current();
    CHECK(first_v0.version == 0);
    CHECK(first_v0.routes.front().target == "first");
    static_cast<void>(second.current());             // must NOT replace first's pin
    CHECK(first.reload(Config{0, {{"/", "updated"}}}));
    CHECK(first_v0.version == 0);                    // old ref still valid
    CHECK(first_v0.routes.front().target == "first"); // unchanged (immutable)
    // current() now reflects the new version
    CHECK(first.current().version == 1);
}

// 10. current() reference stable until next current() on same instance/thread.
static void test_current_stable_until_next_call() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    const Config& v0 = config.current();
    const Config& v1addr = config.current();
    // Same call returns ref to the pinned snapshot; still v0.
    CHECK(v0.version == 0);
    (void)v1addr;
    CHECK(config.reload(Config{0, {{"/", "b"}}}));
    // v0 must remain valid & unchanged after reload.
    CHECK(v0.version == 0);
    CHECK(v0.routes.front().target == "a");
    const Config& v2 = config.current();
    CHECK(v2.version == 1);
}

// 11. Concurrent reload / snapshot / find_target / current — no crash, no UAF.
static void test_concurrent() {
    RoutingConfig config(Config{0, {{"/", "a"}, {"/v1", "b"}}});
    std::atomic<bool> stop{false};
    std::vector<std::thread> ts;
    // reloader
    ts.emplace_back([&] {
        int n = 0;
        while (!stop.load()) {
            config.reload(Config{0, {{"/", "x" + std::to_string(n)}, {"/v1", "y"}}});
            ++n;
        }
    });
    // readers
    for (int i = 0; i < 3; ++i) {
        ts.emplace_back([&] {
            while (!stop.load()) {
                auto s = config.snapshot();
                CHECK(s->version <= static_cast<std::uint64_t>(1'000'000));
                std::string t = config.find_target("/v1/users");
                CHECK(t == "y" || !t.empty());
                const Config& c = config.current();
                (void)c.version;
            }
        });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    stop.store(true);
    for (auto& t : ts) t.join();
}

int main() {
    test_validation();
    test_version_and_snapshot();
    test_find_target();
    test_observer_notify();
    test_observer_exception_isolation();
    test_observer_reentrancy();
    test_new_observer_in_callback();
    test_observer_destroy_reentry();
    test_cross_instance_current();
    test_current_stable_until_next_call();
    test_concurrent();
    if (failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURES\n", failures);
    return 1;
}
