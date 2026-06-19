#include "routing_config.hpp"

#include <atomic>
#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

int main() {
    {
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

    {
        RoutingConfig config(Config{0, {}});
        assert(!config.reload(Config{0, {{"", "target"}}}));
        assert(!config.reload(Config{0, {{"/", ""}}}));
        assert(!config.reload(Config{0, {{"/a", "x"}, {"/a", "y"}}}));
        assert(config.current().version == 0);
    }

    {
        RoutingConfig config(Config{5, {{"/", "old"}}});
        const Config& ref = config.current();
        assert(ref.version == 5);
        config.reload(Config{0, {{"/", "new"}}});
        assert(ref.version == 5);
        const Config& ref2 = config.current();
        assert(ref2.version == 6);
    }

    {
        RoutingConfig config(Config{0, {{"/", "default"}}});
        int notify_count = 0;
        auto id = config.subscribe([&notify_count](std::shared_ptr<const Config>) {
            ++notify_count;
        });
        config.unsubscribe(id);
        config.reload(Config{0, {{"/", "updated"}}});
        assert(notify_count == 0);
    }

    {
        RoutingConfig config(Config{0, {{"/", "default"}}});
        int ok_count = 0;
        config.subscribe([&ok_count](std::shared_ptr<const Config>) {
            ++ok_count;
        });
        config.subscribe([](std::shared_ptr<const Config>) {
            throw std::runtime_error("observer error");
        });
        config.subscribe([&ok_count](std::shared_ptr<const Config>) {
            ++ok_count;
        });
        config.reload(Config{0, {{"/", "v1"}}});
        assert(ok_count == 2);
        assert(config.observer_error_count() == 1);
    }

    {
        RoutingConfig config(Config{0, {{"/", "default"}}});
        int reentrant_reload_count = 0;
        config.subscribe([&](std::shared_ptr<const Config> snap) {
            if (snap->version == 1 && reentrant_reload_count < 2) {
                ++reentrant_reload_count;
                config.reload(Config{0, {{"/", "v2"}, {"/v3", "extra"}}});
            }
        });
        config.reload(Config{0, {{"/", "v1"}}});
        assert(config.current().version == 2);
        assert(reentrant_reload_count == 1);
    }

    {
        RoutingConfig config(Config{0, {{"/", "default"}}});
        config.subscribe([&](std::shared_ptr<const Config>) {
            (void)config.snapshot();
            (void)config.find_target("/anything");
            (void)config.observer_error_count();
        });
        config.reload(Config{0, {{"/", "v1"}}});
        assert(config.current().version == 1);
    }

    {
        RoutingConfig config(Config{0, {{"/api", "api-service"}, {"/api/v2", "api-v2"}}});
        assert(config.find_target("/api/v2/users") == "api-v2");
        assert(config.find_target("/api/v1/users") == "api-service");
        assert(config.find_target("/other") == "");
    }

    {
        RoutingConfig config(Config{0, {{"/", "default"}}});
        std::vector<std::thread> threads;
        std::atomic<int> reloads_done{0};
        const int num_threads = 4;
        const int reloads_per_thread = 50;

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t] {
                for (int i = 0; i < reloads_per_thread; ++i) {
                    std::string prefix = "/t" + std::to_string(t) + "r" + std::to_string(i);
                    config.reload(Config{0, {{"/", "default"}, {prefix, "target"}}});
                    ++reloads_done;
                }
            });
        }
        for (auto& t : threads) t.join();
        assert(reloads_done.load() == num_threads * reloads_per_thread);
        assert(config.current().version == static_cast<std::uint64_t>(num_threads * reloads_per_thread));
    }

    {
        RoutingConfig config(Config{0, {{"/", "default"}}});
        int new_sub_count = 0;
        config.subscribe([&](std::shared_ptr<const Config>) {
            config.subscribe([&new_sub_count](std::shared_ptr<const Config>) {
                ++new_sub_count;
            });
        });
        config.reload(Config{0, {{"/", "v1"}}});
        assert(new_sub_count == 0);
        config.reload(Config{0, {{"/", "v2"}}});
        assert(new_sub_count == 1);
    }

    return 0;
}
