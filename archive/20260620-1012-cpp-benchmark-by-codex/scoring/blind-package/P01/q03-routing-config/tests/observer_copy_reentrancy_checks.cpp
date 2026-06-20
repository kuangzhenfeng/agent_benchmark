#include "routing_config.hpp"

#include <cassert>

namespace {

struct ReenterOnCopy {
    RoutingConfig* config;
    bool* armed;

    ReenterOnCopy(RoutingConfig* config, bool* armed)
        : config(config), armed(armed) {}

    ReenterOnCopy(const ReenterOnCopy& other)
        : config(other.config), armed(other.armed) {
        if (*armed) {
            static_cast<void>(config->snapshot());
        }
    }

    void operator()(std::shared_ptr<const Config>) const {}
};

}  // namespace

int main() {
    RoutingConfig config(Config{0, {{"/", "default"}}});
    bool armed = false;
    config.subscribe(ReenterOnCopy{&config, &armed});
    armed = true;

    // Copying the stored Observer under the config mutex deadlocks here.
    // Copying a wrapper pointer under the mutex and invoking the observer only
    // after unlock passes.
    assert(config.reload(Config{0, {{"/", "blue"}}}));
}
