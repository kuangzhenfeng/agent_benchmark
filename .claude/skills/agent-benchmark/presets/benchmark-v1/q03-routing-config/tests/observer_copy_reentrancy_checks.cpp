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

    // 在 config mutex 下复制已存储的 Observer 会在这里死锁。
    // 改为在锁下只复制 wrapper 指针、解锁后再调用 observer 才能通过。
    assert(config.reload(Config{0, {{"/", "blue"}}}));
}
