#include "routing_config.hpp"

#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    {
        RoutingConfig config(Config{0, {{"/", "default"}}});
        const Config& before = config.current();
        assert(config.reload(Config{0, {{"/", "green"}}}));

        // 旧引用需要在同线程下一次 current() 前保持有效。
        assert(before.version == 0);
        assert(before.routes.front().target == "default");

        const Config& after = config.current();
        assert(after.version == 1);
        assert(after.routes.front().target == "green");
    }

    {
        RoutingConfig config(Config{0, {{"/", "default"}}});
        std::vector<std::uint64_t> first_versions;
        std::vector<std::uint64_t> second_versions;
        std::vector<std::uint64_t> late_versions;

        RoutingConfig::ObserverId second_id = 0;
        config.subscribe([&](std::shared_ptr<const Config> snapshot) {
            first_versions.push_back(snapshot->version);
            if (snapshot->version == 1) {
                config.unsubscribe(second_id);
                config.subscribe([&](std::shared_ptr<const Config> late_snapshot) {
                    late_versions.push_back(late_snapshot->version);
                });
            }
        });
        second_id = config.subscribe([&](std::shared_ptr<const Config> snapshot) {
            second_versions.push_back(snapshot->version);
        });
        config.subscribe([](std::shared_ptr<const Config>) {
            throw std::runtime_error("observer failed");
        });

        assert(config.reload(Config{0, {{"/", "green"}, {"/v2", "blue"}}}));
        assert(config.reload(Config{0, {{"/", "gold"}, {"/v10", "canary"}}}));

        assert((first_versions == std::vector<std::uint64_t>{1, 2}));
        assert((second_versions == std::vector<std::uint64_t>{1}));
        assert((late_versions == std::vector<std::uint64_t>{2}));
        assert(config.observer_error_count() == 2);
        assert(config.find_target("/v10/users") == "canary");
        assert(config.find_target("/v2/users") == "gold");
    }
}
