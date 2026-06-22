#include "routing_config.hpp"

#include <cassert>

int main() {
    RoutingConfig config(Config{0, {{"/", "default"}, {"/v1", "blue"}}});
    int good_calls = 0;
    int late_calls = 0;
    bool added_late = false;

    config.subscribe([](std::shared_ptr<const Config>) { throw 1; });
    config.subscribe([&](std::shared_ptr<const Config> published) {
        ++good_calls;
        assert(published->version == static_cast<std::uint64_t>(good_calls));
        if (!added_late) {
            added_late = true;
            config.subscribe([&](std::shared_ptr<const Config>) { ++late_calls; });
        }
    });

    assert(config.reload(Config{99, {{"/", "default"}, {"/v1", "green"}}}));
    assert(good_calls == 1);
    assert(late_calls == 0);
    assert(config.observer_error_count() == 1);
    assert(config.find_target("/v1/users") == "green");

    const auto before_invalid = config.snapshot();
    assert(!config.reload(Config{0, {{"", "invalid"}}}));
    assert(config.snapshot() == before_invalid);

    assert(config.reload(Config{0, {{"/", "default"}, {"/v1", "canary"}}}));
    assert(good_calls == 2);
    assert(late_calls == 1);
    assert(config.observer_error_count() == 2);
}
