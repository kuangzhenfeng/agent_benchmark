#include "subscription_hub.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

int main() {
    SubscriptionHub hub;
    std::vector<std::string> first_received;
    std::vector<std::string> second_received;
    std::vector<std::string> late_received;

    SubscriptionHub::SubscriptionId second_id = 0;
    hub.subscribe(std::string("orders"),
                  [&](const SubscriptionHub::Event& event) {
                      first_received.emplace_back(event.payload);
                      if (event.sequence == 1) {
                          hub.unsubscribe(second_id);
                          hub.subscribe("orders",
                                        [&](const SubscriptionHub::Event& late_event) {
                                            late_received.emplace_back(late_event.payload);
                                        },
                                        1);
                      }
                  },
                  0);
    second_id = hub.subscribe(std::string("orders"),
                              [&](const SubscriptionHub::Event& event) {
                                  second_received.emplace_back(event.payload);
                              },
                              0);
    hub.subscribe("orders",
                  [](const SubscriptionHub::Event&) {
                      throw 7;
                  },
                  0);

    hub.publish(std::string("orders"), std::string("created"));
    hub.publish(std::string("orders"), std::string("updated"));
    const auto stats = hub.drain(1);

    assert(stats.delivered == 5);
    assert(stats.callback_errors == 2);
    assert((first_received == std::vector<std::string>{"created", "updated"}));
    assert((second_received == std::vector<std::string>{"created"}));
    assert(late_received.empty());
    assert(hub.pending_events() == 0);

    SubscriptionHub wrap_hub;
    wrap_hub.subscribe("wrap", [](const SubscriptionHub::Event&) {}, 
                       std::numeric_limits<std::uint32_t>::max() - 3);
    assert(wrap_hub.expire_idle(2, 6) == 1);

    SubscriptionHub zero_ttl_hub;
    zero_ttl_hub.subscribe("ttl0", [](const SubscriptionHub::Event&) {}, 10);
    assert(zero_ttl_hub.expire_idle(10, 0) == 1);
}
