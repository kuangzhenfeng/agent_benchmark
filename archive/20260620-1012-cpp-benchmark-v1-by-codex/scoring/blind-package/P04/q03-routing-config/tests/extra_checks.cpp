#include "routing_config.hpp"

#include <atomic>
#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

void test_validation_failures_unchanged() {
    RoutingConfig config(Config{0, {{"/", "default"}}});
    assert(config.reload(Config{0, {{"/", "a"}, {"/", "b"}}}) == false);  // dup prefix
    assert(config.reload(Config{0, {{"/", ""}}}) == false);               // empty target
    assert(config.reload(Config{0, {{"", "x"}}}) == false);              // empty prefix
    assert(config.current().version == 0);  // unchanged
    assert(config.find_target("/x") == "default");
}

void test_version_strictly_increments() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    for (int i = 1; i <= 5; ++i) {
        assert(config.reload(Config{0, {{"/", std::to_string(i)}}}));
        assert(config.current().version == static_cast<std::uint64_t>(i));
    }
    // Failed reload does not bump version.
    assert(!config.reload(Config{0, {{"/", "x"}, {"/", "y"}}}));
    assert(config.current().version == 5);
}

void test_find_target_longest_prefix() {
    RoutingConfig config(Config{0, {
        {"/", "root"},
        {"/v1", "v1"},
        {"/v1/users", "users"},
        {"/v1/users/admin", "admin"},
    }});
    assert(config.find_target("/v1/users/admin/x") == "admin");
    assert(config.find_target("/v1/users/other") == "users");
    assert(config.find_target("/v1/groups") == "v1");
    assert(config.find_target("/health") == "root");
    assert(config.find_target("/v1users") == "v1");  // prefix match on /v1
    // "/" matches everything; a config without "/" yields empty for misses.
    RoutingConfig c2(Config{0, {{"/api", "a"}}});
    assert(c2.find_target("/other").empty());
}

void test_observer_exception_isolation() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    std::vector<std::uint64_t> seen;
    config.subscribe([](std::shared_ptr<const Config>) { throw std::runtime_error("boom"); });
    config.subscribe([&seen](std::shared_ptr<const Config> s) { seen.push_back(s->version); });
    config.subscribe([](std::shared_ptr<const Config>) { throw 42; });

    assert(config.reload(Config{0, {{"/", "b"}}}));
    // The middle observer still received the snapshot.
    assert(seen.size() == 1 && seen[0] == 1);
    // Two observers threw.
    assert(config.observer_error_count() == 2);
    // Config still published (no rollback).
    assert(config.current().version == 1);
}

void test_observer_unsubscribe_during_callback_affects_future_only() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    int a_count = 0, b_count = 0;
    RoutingConfig::ObserverId b_id = 0;
    auto a_id = config.subscribe([&](std::shared_ptr<const Config>) {
        ++a_count;
        if (b_id != 0) config.unsubscribe(b_id);  // unsub b during notify
    });
    (void)a_id;
    b_id = config.subscribe([&](std::shared_ptr<const Config>) { ++b_count; });

    // First reload: both subscribed at start -> both notified (b unsub'd mid-way
    // but still counts for THIS reload per "subscribed at reload start").
    assert(config.reload(Config{0, {{"/", "b"}}}));
    assert(a_count == 1);
    assert(b_count == 1);  // was subscribed at reload start
    // Second reload: b is gone.
    assert(config.reload(Config{0, {{"/", "c"}}}));
    assert(a_count == 2);
    assert(b_count == 1);
}

void test_new_subscription_during_callback_no_current_notify() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    int a_count = 0, new_count = 0;
    config.subscribe([&](std::shared_ptr<const Config> s) {
        ++a_count;
        // Subscribe a new observer during notification.
        config.subscribe([&new_count](std::shared_ptr<const Config>) { ++new_count; });
        (void)s;
    });
    assert(config.reload(Config{0, {{"/", "b"}}}));
    assert(a_count == 1);
    assert(new_count == 0);  // new observer didn't get this notification
    // Next reload: new observer (now present) is notified.
    assert(config.reload(Config{0, {{"/", "c"}}}));
    assert(a_count == 2);
    assert(new_count == 1);
}

void test_reentrant_reload_in_observer() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    int nested_version = -1;
    config.subscribe([&](std::shared_ptr<const Config> s) {
        // Observer re-enters reload; must not deadlock.
        if (s->version == 1) {
            config.reload(Config{0, {{"/", "c"}}});
            nested_version = static_cast<int>(config.snapshot()->version);
        }
    });
    assert(config.reload(Config{0, {{"/", "b"}}}));
    assert(nested_version == 2);
    assert(config.current().version == 2);
}

void test_snapshot_consistency_under_concurrency() {
    RoutingConfig config(Config{0, {{"/", "a"}, {"/v", "b"}}});
    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;
    // Writer: repeatedly reload.
    threads.emplace_back([&] {
        int i = 0;
        while (!stop.load()) {
            config.reload(Config{0, {{"/", "a"}, {"/v", std::to_string(i++ % 5)}}});
        }
    });
    // Readers: snapshot + find_target must be self-consistent.
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 20000; ++i) {
                auto snap = config.snapshot();
                // snapshot is immutable & self-consistent; just exercise it.
                const std::string v = snap->routes.size() >= 2 ? snap->routes[1].target : "";
                (void)v;
                config.find_target("/v");
            }
        });
    }
    for (int i = 0; i < 20000; ++i) config.find_target("/");
    stop.store(true);
    for (auto& th : threads) th.join();
    // No assertion beyond "did not crash / no hang".
    assert(config.current().version > 0);
}

void test_current_reference_survives_many_reloads() {
    RoutingConfig config(Config{0, {{"/", "init"}}});
    const Config& ref = config.current();
    assert(ref.version == 0);
    for (int i = 1; i <= 50; ++i) {
        config.reload(Config{0, {{"/", std::to_string(i)}}});
    }
    // The original reference must still point at the v0 snapshot.
    assert(ref.version == 0);
    assert(ref.routes.front().target == "init");
    assert(config.current().version == 50);
}

void test_observer_destructor_runs_outside_lock() {
    // Exercise unsubscribe's "drop outside lock" path with a non-trivial
    // observer; the destructor must not run under the mutex. We approximate
    // the guarantee by checking the instance stays usable after unsubscribe.
    RoutingConfig config(Config{0, {{"/", "a"}}});
    auto id = config.subscribe([captured = std::string(64, 'x')](
        std::shared_ptr<const Config>) { (void)captured; });
    config.unsubscribe(id);
    // Repeated subscribe/unsubscribe + reload after destruction.
    for (int i = 0; i < 10; ++i) {
        auto x = config.subscribe([](std::shared_ptr<const Config>) {});
        config.unsubscribe(x);
    }
    assert(config.reload(Config{0, {{"/", "b"}}}));
    assert(config.current().version == 1);
}

}  // namespace

int main() {
    test_validation_failures_unchanged();
    test_version_strictly_increments();
    test_find_target_longest_prefix();
    test_observer_exception_isolation();
    test_observer_unsubscribe_during_callback_affects_future_only();
    test_new_subscription_during_callback_no_current_notify();
    test_reentrant_reload_in_observer();
    test_snapshot_consistency_under_concurrency();
    test_current_reference_survives_many_reloads();
    test_observer_destructor_runs_outside_lock();
    return 0;
}
