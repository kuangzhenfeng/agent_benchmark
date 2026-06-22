#include "subscription_hub.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

int main() {
    SubscriptionHub hub;
    std::vector<std::string> received;
    SubscriptionHub::SubscriptionId cancelled = 0;

    hub.subscribe(std::string("orders"), [&](const SubscriptionHub::Event& event) {
        received.push_back("first:" + std::string(event.payload));
        if (event.payload == "one") {
            hub.unsubscribe(cancelled);
            hub.subscribe("orders", [&](const SubscriptionHub::Event& late) {
                received.push_back("late:" + std::string(late.payload));
            }, 1);
        }
    }, 0);
    cancelled = hub.subscribe("orders", [&](const SubscriptionHub::Event& event) {
        received.push_back("cancelled:" + std::string(event.payload));
    }, 0);
    hub.subscribe("orders", [](const SubscriptionHub::Event&) {
        throw 1;
    }, 0);

    hub.publish(std::string("orders"), std::string("one"));
    hub.publish("orders", "two");
    const auto first_batch = hub.drain(1);
    assert(first_batch.delivered == 5);
    assert(first_batch.callback_errors == 2);
    assert((received == std::vector<std::string>{
        "first:one", "cancelled:one", "first:two"}));

    hub.publish("orders", "three");
    const auto second_batch = hub.drain(2);
    assert(second_batch.delivered == 3);
    assert(second_batch.callback_errors == 1);
    assert(received.back() == "late:three");

    SubscriptionHub wrap_hub;
    wrap_hub.subscribe("idle", [](const SubscriptionHub::Event&) {},
                       UINT32_MAX - 1U);
    assert(wrap_hub.expire_idle(1U, 2U) == 1);
}
