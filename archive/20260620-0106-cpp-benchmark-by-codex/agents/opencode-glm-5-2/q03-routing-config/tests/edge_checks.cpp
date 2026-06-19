#include "routing_config.hpp"

#include <atomic>
#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

static void test_current_ref_valid_through_reload() {
    RoutingConfig config(Config{0, {{"/", "default"}}});
    const Config& ref = config.current();
    assert(ref.version == 0);
    const std::string* old_target = &ref.routes[0].target;
    assert(*old_target == "default");

    assert(config.reload(Config{0, {{"/", "new"}, {"/v1", "blue"}}}));
    assert(ref.version == 0);
    assert(ref.routes[0].target == "default");
    assert(*old_target == "default");

    const Config& ref2 = config.current();
    assert(ref2.version == 1);
    assert(ref2.routes[0].target == "new");
}

static void test_snapshot_is_consistent() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    auto snap1 = config.snapshot();
    assert(snap1->version == 0);

    config.reload(Config{0, {{"/", "b"}}});
    assert(snap1->version == 0);
    assert(snap1->routes[0].target == "a");

    auto snap2 = config.snapshot();
    assert(snap2->version == 1);
    assert(snap2->routes[0].target == "b");
}

static void test_validation_failures_unchanged() {
    RoutingConfig config(Config{5, {{"/", "default"}}});
    const auto version_before = config.snapshot()->version;
    const auto obs_before = config.observer_error_count();

    assert(!config.reload(Config{0, {{"", "empty-prefix"}}}));
    assert(!config.reload(Config{0, {{"/no-target", ""}}}));
    assert(!config.reload(Config{0, {{"/dup", "a"}, {"/dup", "b"}}}));

    assert(config.snapshot()->version == version_before);
    assert(config.observer_error_count() == obs_before);
}

static void test_version_strictly_increments() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    config.reload(Config{0, {{"/", "b"}}});
    assert(config.snapshot()->version == 1);
    config.reload(Config{99, {{"/", "c"}}});
    assert(config.snapshot()->version == 2);
    config.reload(Config{0, {{"/", "d"}}});
    assert(config.snapshot()->version == 3);
}

static void test_observer_exception_does_not_block() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    std::atomic<int> call_count{0};
    config.subscribe([&call_count](std::shared_ptr<const Config>) {
        ++call_count;
        throw std::runtime_error("boom");
    });
    std::atomic<int> second_count{0};
    config.subscribe([&second_count](std::shared_ptr<const Config>) {
        ++second_count;
    });

    assert(config.reload(Config{0, {{"/", "b"}}}));
    assert(call_count.load() == 1);
    assert(second_count.load() == 1);
    assert(config.observer_error_count() == 1);
    assert(config.snapshot()->version == 1);
}

static void test_observer_exception_does_not_rollback() {
    RoutingConfig config(Config{0, {{"/", "original"}}});
    config.subscribe([](std::shared_ptr<const Config>) {
        throw std::runtime_error("always");
    });

    assert(config.reload(Config{0, {{"/", "new"}}}));
    assert(config.snapshot()->version == 1);
    assert(config.find_target("/x") == "new");
    assert(config.observer_error_count() == 1);
}

static void test_reentrant_subscribe_does_not_receive_current() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    std::atomic<int> initial_count{0};
    std::atomic<int> new_count{0};
    RoutingConfig::ObserverId new_id = 0;

    config.subscribe([&initial_count, &new_count, &new_id, &config](std::shared_ptr<const Config>) {
        ++initial_count;
        new_id = config.subscribe([&new_count](std::shared_ptr<const Config>) {
            ++new_count;
        });
    });

    assert(config.reload(Config{0, {{"/", "b"}}}));
    assert(initial_count.load() == 1);
    assert(new_count.load() == 0);
    assert(new_id != 0);

    assert(config.reload(Config{0, {{"/", "c"}}}));
    assert(initial_count.load() == 2);
    assert(new_count.load() == 1);
}

static void test_reentrant_unsubscribe_affects_future_only() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    std::atomic<int> a_count{0};
    std::atomic<int> b_count{0};
    RoutingConfig::ObserverId a_id = 0;

    a_id = config.subscribe([&a_count, &a_id, &config](std::shared_ptr<const Config> snap) {
        ++a_count;
        if (snap->version == 1) {
            (void)a_id;
        }
    });
    config.subscribe([&a_count, &a_id, &config, &b_count](std::shared_ptr<const Config> snap) {
        ++b_count;
        if (snap->version == 1) {
            config.unsubscribe(a_id);
        }
    });

    assert(config.reload(Config{0, {{"/", "b"}}}));
    assert(a_count.load() == 1);
    assert(b_count.load() == 1);

    assert(config.reload(Config{0, {{"/", "c"}}}));
    assert(a_count.load() == 1);
    assert(b_count.load() == 2);
}

static void test_reentrant_reload_during_notification() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    std::atomic<int> notify_count{0};
    std::vector<std::uint64_t> seen_versions;
    seen_versions.reserve(4);

    config.subscribe([&notify_count, &seen_versions, &config](std::shared_ptr<const Config> snap) {
        ++notify_count;
        seen_versions.push_back(snap->version);
        if (snap->version == 1) {
            config.reload(Config{0, {{"/", "c"}}});
        }
    });

    assert(config.reload(Config{0, {{"/", "b"}}}));
    assert(notify_count.load() == 2);
    assert(config.snapshot()->version == 2);
}

static void test_reentrant_snapshot_during_notification() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    std::shared_ptr<const Config> snap_in_cb;
    config.subscribe([&snap_in_cb, &config](std::shared_ptr<const Config>) {
        snap_in_cb = config.snapshot();
    });

    config.reload(Config{0, {{"/", "b"}}});
    assert(snap_in_cb);
    assert(snap_in_cb->version == 1);
}

static void test_longest_prefix_match() {
    RoutingConfig config(Config{0, {
        {"/", "root"},
        {"/api", "api"},
        {"/api/v1", "v1"},
        {"/api/v1/users", "users"},
    }});

    assert(config.find_target("/api/v1/users/123") == "users");
    assert(config.find_target("/api/v1/orders") == "v1");
    assert(config.find_target("/api/v2") == "api");
    assert(config.find_target("/health") == "root");
    assert(config.find_target("/api/v1/users") == "users");
}

static void test_no_match_returns_empty() {
    RoutingConfig config(Config{0, {{"/api", "api"}}});
    assert(config.find_target("/health").empty());
    assert(config.find_target("").empty());
}

static void test_find_target_consistent_snapshot() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    for (int i = 0; i < 10; ++i) {
        config.reload(Config{0, {{"/", "v" + std::to_string(i)}}});
    }
    std::string result = config.find_target("/test");
    assert(result.substr(0, 1) == "v");
}

static void test_concurrent_reload_snapshot_findtarget() {
    RoutingConfig config(Config{0, {{"/", "init"}, {"/api", "api"}}});
    std::atomic<bool> stop{false};
    std::atomic<int> reloads{0};

    std::vector<std::thread> threads;
    threads.emplace_back([&]() {
        while (!stop.load()) {
            if (config.reload(Config{0, {{"/", "r"}, {"/api", "api"}, {"/api/v2", "v2"}}})) {
                ++reloads;
            }
            if (config.reload(Config{0, {{"/", "r"}, {"/api", "api"}}})) {
                ++reloads;
            }
        }
    });
    threads.emplace_back([&]() {
        while (!stop.load()) {
            auto snap = config.snapshot();
            assert(snap->routes.size() >= 1);
        }
    });
    threads.emplace_back([&]() {
        while (!stop.load()) {
            std::string t = config.find_target("/api/v2/something");
            assert(!t.empty());
        }
    });
    threads.emplace_back([&]() {
        while (!stop.load()) {
            const Config& c = config.current();
            assert(c.routes.size() >= 1);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop.store(true);
    for (auto& t : threads) t.join();

    assert(reloads.load() > 0);
    assert(config.snapshot()->version > 0);
}

static void test_each_observer_notified_once_per_reload() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    std::atomic<int> count_a{0};
    std::atomic<int> count_b{0};
    config.subscribe([&count_a](std::shared_ptr<const Config>) { ++count_a; });
    config.subscribe([&count_b](std::shared_ptr<const Config>) { ++count_b; });

    config.reload(Config{0, {{"/", "b"}}});
    assert(count_a.load() == 1);
    assert(count_b.load() == 1);

    config.reload(Config{0, {{"/", "c"}}});
    assert(count_a.load() == 2);
    assert(count_b.load() == 2);
}

static void test_observer_receives_published_snapshot() {
    RoutingConfig config(Config{0, {{"/", "a"}}});
    std::shared_ptr<const Config> received;
    config.subscribe([&received](std::shared_ptr<const Config> snap) {
        received = snap;
    });
    config.reload(Config{0, {{"/", "b"}, {"/x", "y"}}});
    assert(received);
    assert(received->version == 1);
    assert(received->routes.size() == 2);
    assert(received->routes[0].target == "b");
    auto same = config.snapshot();
    assert(same->version == received->version);
}

int main() {
    test_current_ref_valid_through_reload();
    test_snapshot_is_consistent();
    test_validation_failures_unchanged();
    test_version_strictly_increments();
    test_observer_exception_does_not_block();
    test_observer_exception_does_not_rollback();
    test_reentrant_subscribe_does_not_receive_current();
    test_reentrant_unsubscribe_affects_future_only();
    test_reentrant_reload_during_notification();
    test_reentrant_snapshot_during_notification();
    test_longest_prefix_match();
    test_no_match_returns_empty();
    test_find_target_consistent_snapshot();
    test_concurrent_reload_snapshot_findtarget();
    test_each_observer_notified_once_per_reload();
    test_observer_receives_published_snapshot();
    return 0;
}
