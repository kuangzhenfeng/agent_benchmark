#include "routing_config.hpp"

#include <atomic>
#include <cassert>
#include <string>
#include <thread>
#include <vector>

void test_validation_failure_no_change() {
    RoutingConfig config(Config{0, {{"/", "default"}}});
    assert(!config.reload(Config{0, {{"/", "a"}, {"/", "b"}}}));
    assert(!config.reload(Config{0, {{"/", ""}}}));
    assert(config.current().version == 0);
    assert(config.snapshot()->version == 0);
}

void test_version_increment() {
    RoutingConfig config(Config{0, {{"/", "default"}}});
    assert(config.reload(Config{0, {{"/", "v1"}}}));
    assert(config.current().version == 1);
    assert(config.reload(Config{0, {{"/", "v2"}}}));
    assert(config.current().version == 2);
}

void test_longest_prefix_match() {
    RoutingConfig config(Config{0, {{"/", "default"}, {"/v1", "blue"}, {"/v1/api", "api"}}});
    assert(config.find_target("/v1/api/users") == "api");
    assert(config.find_target("/v1/users") == "blue");
    assert(config.find_target("/status") == "default");
    assert(config.find_target("no_match") == "");
}

void test_current_reference_stability_after_reload() {
    RoutingConfig config(Config{0, {{"/", "first"}}});
    const Config& ref1 = config.current();
    assert(ref1.version == 0);
    assert(ref1.routes.front().target == "first");

    assert(config.reload(Config{0, {{"/", "updated"}}}));
    assert(config.current().version == 1);

    assert(ref1.version == 0);
    assert(ref1.routes.front().target == "first");
}

void test_observer_exception_isolation() {
    RoutingConfig config(Config{0, {{"/", "default"}}});
    std::atomic<int> ok_count{0};

    config.subscribe([&ok_count](std::shared_ptr<const Config>) {
        throw std::runtime_error("observer error");
    });
    config.subscribe([&ok_count](std::shared_ptr<const Config>) {
        ++ok_count;
    });

    assert(config.reload(Config{0, {{"/", "new"}}}));
    assert(config.current().version == 1);
    assert(ok_count == 1);
    assert(config.observer_error_count() == 1);
}

void test_observer_unsubscribe_during_notification() {
    RoutingConfig config(Config{0, {{"/", "default"}}});
    RoutingConfig::ObserverId id1 = 0;
    std::atomic<int> notify_count{0};

    id1 = config.subscribe([&config, &id1, &notify_count](std::shared_ptr<const Config>) {
        ++notify_count;
        config.unsubscribe(id1);
    });
    config.subscribe([&notify_count](std::shared_ptr<const Config>) {
        ++notify_count;
    });

    assert(config.reload(Config{0, {{"/", "new"}}}));
    assert(notify_count == 2);
}

void test_observer_subscribe_during_notification() {
    RoutingConfig config(Config{0, {{"/", "default"}}});
    std::atomic<int> existing_notify{0};
    std::atomic<int> new_notify{0};
    RoutingConfig::ObserverId new_id = 0;

    config.subscribe([&config, &existing_notify, &new_id, &new_notify](std::shared_ptr<const Config>) {
        ++existing_notify;
        new_id = config.subscribe([&new_notify](std::shared_ptr<const Config>) {
            ++new_notify;
        });
    });

    assert(config.reload(Config{0, {{"/", "new"}}}));
    assert(existing_notify == 1);
    assert(new_notify == 0);

    assert(config.reload(Config{0, {{"/", "new2"}}}));
    assert(new_notify == 1);
}

void test_concurrent_reload_snapshot() {
    RoutingConfig config(Config{0, {{"/", "default"}}});

    std::vector<std::thread> threads;
    std::atomic<int> reload_count{0};
    std::atomic<int> snapshot_count{0};

    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&config, &reload_count, i] {
            for (int j = 0; j < 10; ++j) {
                config.reload(Config{0, {{"/", "route" + std::to_string(i * 10 + j)}}});
                ++reload_count;
            }
        });
    }
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&config, &snapshot_count] {
            for (int j = 0; j < 20; ++j) {
                auto snap = config.snapshot();
                assert(snap->routes.size() >= 1);
                ++snapshot_count;
            }
        });
    }

    for (auto& t : threads) t.join();
    assert(reload_count == 40);
    assert(snapshot_count == 80);
}

int main() {
    test_validation_failure_no_change();
    test_version_increment();
    test_longest_prefix_match();
    test_current_reference_stability_after_reload();
    test_observer_exception_isolation();
    test_observer_unsubscribe_during_notification();
    test_observer_subscribe_during_notification();
    test_concurrent_reload_snapshot();
    return 0;
}
