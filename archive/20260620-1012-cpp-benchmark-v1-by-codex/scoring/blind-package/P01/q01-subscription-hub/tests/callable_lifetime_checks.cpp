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

    // The full expression completes before arming, so only destruction of the
    // hub-owned callback is relevant below.
    control.id = hub.subscribe("orders", ReenterOnDestroy{&control}, 0);
    control.armed.store(true, std::memory_order_relaxed);

    // A correct implementation releases the final callback owner after
    // unlocking.  Re-entering unsubscribe then observes an already-removed ID.
    hub.unsubscribe(control.id);
}
