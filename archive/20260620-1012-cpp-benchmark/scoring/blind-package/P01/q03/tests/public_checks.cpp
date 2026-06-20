#include "routing_config.hpp"

#include <cassert>
#include <memory>
#include <string>

int main() {
    RoutingConfig config(Config{0, {{"/", "default"}}});
    std::shared_ptr<const Config> observed;
    config.subscribe([&observed](std::shared_ptr<const Config> snapshot) {
        observed = std::move(snapshot);
    });

    assert(config.reload(Config{0, {{"/", "default"}, {"/v1", "blue"}}}));
    assert(config.current().version == 1);
    assert(observed && observed->version == 1);
    assert(config.find_target("/v1/users") == "blue");
    assert(config.find_target("/status") == "default");
    assert(!config.reload(Config{0, {{"/", "a"}, {"/", "b"}}}));
    assert(config.snapshot()->version == 1);
}
