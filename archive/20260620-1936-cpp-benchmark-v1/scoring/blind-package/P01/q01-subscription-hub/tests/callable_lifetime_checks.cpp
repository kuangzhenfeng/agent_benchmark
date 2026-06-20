#include "subscription_hub.hpp"

#include <atomic>

namespace {

struct Control {
    SubscriptionHub* hub = nullptr;
    SubscriptionHub::SubscriptionId id = 0;
    std::atomic<bool> armed{false};
};

struct ReenterOnDestroy {
    Control* control;

    void operator()(const SubscriptionHub::Event&) const {}

    ~ReenterOnDestroy() {
        if (control->armed.load(std::memory_order_relaxed)) {
            control->hub->unsubscribe(control->id);
        }
    }
};

}  // namespace

int main() {
    SubscriptionHub hub;
    Control control{&hub};

    // 完整表达式在 arming 前结束，因此下面只需考察 hub 持有的
    // callback 的析构。
    control.id = hub.subscribe("orders", ReenterOnDestroy{&control}, 0);
    control.armed.store(true, std::memory_order_relaxed);

    // 正确的实现会在解锁后再释放最后一个 callback 所有者。
    // 此后重入 unsubscribe 时观察到的是一个已被移除的 ID。
    hub.unsubscribe(control.id);
}
