#include "routing_config.hpp"

#include <cassert>
#include <memory>
#include <stdexcept>

int main() {
    RoutingConfig config(Config{7, {{"/", "default"}}});
    const Config& retained = config.current();
    std::size_t notified = 0;
    config.subscribe([&](std::shared_ptr<const Config> snapshot) {
        ++notified;
        assert(snapshot->version == 8);
        config.subscribe([](std::shared_ptr<const Config>) {});
    });
    config.subscribe([](std::shared_ptr<const Config>) { throw std::runtime_error("expected"); });

    assert(!config.reload(Config{0, {{"", "bad"}}}));
    assert(config.snapshot()->version == 7);
    assert(notified == 0);

    assert(config.reload(Config{0, {{"/", "default"}, {"/v1", "blue"}}}));
    assert(retained.version == 7);
    assert(config.current().version == 8);
    assert(notified == 1);
    assert(config.observer_error_count() == 1);
    assert(config.find_target("/v1/users") == "blue");
    assert(config.find_target("/other") == "default");
}
