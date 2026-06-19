#include "subscription_hub.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    {
        SubscriptionHub hub;
        std::vector<std::string> delivered;
        hub.subscribe(std::string("temporary-topic"), [&delivered](const auto& event) {
            delivered.emplace_back(event.payload);
        }, 0);
        hub.publish(std::string("temporary-topic"), std::string("temporary-payload"));
        assert(hub.drain(1).delivered == 1);
        assert((delivered == std::vector<std::string>{"temporary-payload"}));
    }

    {
        SubscriptionHub hub;
        std::vector<std::string> delivered;
        hub.subscribe("topic", [&hub, &delivered](const auto& event) {
            delivered.emplace_back(event.payload);
            if (event.payload == "first") {
                hub.publish("topic", "second");
            }
        }, 0);
        hub.publish("topic", "first");
        assert(hub.drain(1).delivered == 1);
        assert(hub.pending_events() == 1);
        assert(hub.drain(2).delivered == 1);
        assert((delivered == std::vector<std::string>{"first", "second"}));
    }

    {
        SubscriptionHub hub;
        std::size_t second_calls = 0;
        SubscriptionHub::SubscriptionId second = 0;
        hub.subscribe("topic", [&hub, &second](const auto&) { hub.unsubscribe(second); }, 0);
        second = hub.subscribe("topic", [&second_calls](const auto&) { ++second_calls; }, 0);
        hub.publish("topic", "one");
        hub.publish("topic", "two");
        assert(hub.drain(1).delivered == 2);
        assert(second_calls == 0);
    }

    {
        SubscriptionHub hub;
        hub.subscribe("topic", [](const auto&) { throw std::runtime_error("expected"); }, 0);
        std::size_t calls = 0;
        hub.subscribe("topic", [&calls](const auto&) { ++calls; }, 0);
        hub.publish("topic", "payload");
        const auto stats = hub.drain(1);
        assert(stats.delivered == 2);
        assert(stats.callback_errors == 1);
        assert(calls == 1);
        assert(hub.pending_events() == 0);
    }

    {
        SubscriptionHub hub;
        hub.subscribe("topic", [](const auto&) {}, UINT32_MAX - 2U);
        assert(hub.expire_idle(1U, 4U) == 1);
    }
}
