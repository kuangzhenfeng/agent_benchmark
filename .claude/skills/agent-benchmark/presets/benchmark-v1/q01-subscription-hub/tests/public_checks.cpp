#include "subscription_hub.hpp"

#include <cassert>
#include <string>
#include <vector>

int main() {
    SubscriptionHub hub;
    std::vector<std::string> received;
    hub.subscribe("orders", [&received](const SubscriptionHub::Event& event) {
        received.emplace_back(event.payload);
    }, 10);

    hub.publish("orders", "created");
    const auto stats = hub.drain(11);

    assert(stats.delivered == 1);
    assert(stats.callback_errors == 0);
    assert((received == std::vector<std::string>{"created"}));
    assert(hub.pending_events() == 0);
}
