#include "subscription_hub.hpp"

#include <atomic>
#include <cassert>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

void test_temporary_string_lifetime() {
    SubscriptionHub hub;
    std::vector<std::string> received;
    hub.subscribe(std::string("orders"), [&received](const SubscriptionHub::Event& event) {
        received.emplace_back(std::string(event.payload));
    }, 0);

    hub.publish(std::string("orders"), std::string("temp_payload"));
    const auto stats = hub.drain(1);
    assert(stats.delivered == 1);
    assert(stats.callback_errors == 0);
    assert(received.size() == 1);
    assert(received[0] == "temp_payload");
}

void test_reentrant_unsubscribe() {
    SubscriptionHub hub;
    std::atomic<int> call_count{0};
    SubscriptionHub::SubscriptionId id;
    id = hub.subscribe("orders", [&hub, &call_count, &id](const SubscriptionHub::Event&) {
        ++call_count;
        hub.unsubscribe(id);
    }, 0);

    hub.publish("orders", "a");
    hub.publish("orders", "b");
    const auto stats = hub.drain(1);
    assert(stats.delivered == 1);
    assert(call_count == 1);
}

void test_reentrant_publish() {
    SubscriptionHub hub;
    std::vector<std::string> received;
    hub.subscribe("orders", [&hub, &received](const SubscriptionHub::Event& event) {
        received.emplace_back(std::string(event.payload));
        if (event.payload == "a") {
            hub.publish("orders", "inner_event");
        }
    }, 0);

    hub.publish("orders", "a");
    const auto stats1 = hub.drain(1);
    assert(stats1.delivered == 1);
    assert(received.size() == 1);
    assert(received[0] == "a");

    const auto stats2 = hub.drain(2);
    assert(stats2.delivered == 1);
    assert(received.size() == 2);
    assert(received[1] == "inner_event");
}

void test_callback_exception_isolation() {
    SubscriptionHub hub;
    std::vector<std::string> received;
    hub.subscribe("orders", [&received](const SubscriptionHub::Event& event) {
        received.emplace_back(std::string(event.payload));
        throw std::runtime_error("test exception");
    }, 0);
    hub.subscribe("orders", [&received](const SubscriptionHub::Event& event) {
        received.emplace_back(std::string(event.payload) + "_ok");
    }, 0);

    hub.publish("orders", "data");
    const auto stats = hub.drain(1);
    assert(stats.delivered == 2);
    assert(stats.callback_errors == 1);
    assert(received.size() == 2);
}

void test_expire_idle_wraparound() {
    SubscriptionHub hub;
    hub.subscribe("a", [](const SubscriptionHub::Event&) {}, 0xFFFFFFF0);
    hub.subscribe("b", [](const SubscriptionHub::Event&) {}, 0xFFFFFFF0);

    std::size_t expired = hub.expire_idle(0x00000010, 0x10);
    assert(expired == 2);
}

void test_expire_idle_ttl_zero() {
    SubscriptionHub hub;
    hub.subscribe("a", [](const SubscriptionHub::Event&) {}, 100);
    std::size_t expired = hub.expire_idle(100, 0);
    assert(expired == 1);
}

void test_reentrant_subscribe_no_current_events() {
    SubscriptionHub hub;
    std::vector<std::string> received_existing;
    std::vector<std::string> received_new;
    hub.subscribe("orders", [&hub, &received_existing, &received_new](const SubscriptionHub::Event& event) {
        received_existing.emplace_back(std::string(event.payload));
        hub.subscribe("orders", [&received_new](const SubscriptionHub::Event& event) {
            received_new.emplace_back(std::string(event.payload));
        }, 0);
    }, 0);

    hub.publish("orders", "first");
    hub.drain(1);
    assert(received_existing.size() == 1);
    assert(received_new.empty());

    hub.publish("orders", "second");
    hub.drain(2);
    assert(received_existing.size() == 2);
    assert(received_new.size() == 1);
}

void test_concurrent_publish_drain() {
    SubscriptionHub hub;
    std::atomic<int> total_delivered{0};
    hub.subscribe("orders", [&total_delivered](const SubscriptionHub::Event&) {
        ++total_delivered;
    }, 0);

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&hub, i] {
            for (int j = 0; j < 10; ++j) {
                hub.publish("orders", std::to_string(i * 10 + j));
            }
        });
    }
    for (auto& t : threads) t.join();

    auto stats = hub.drain(1);
    assert(stats.delivered == 40);
}

int main() {
    test_temporary_string_lifetime();
    test_reentrant_unsubscribe();
    test_reentrant_publish();
    test_callback_exception_isolation();
    test_expire_idle_wraparound();
    test_expire_idle_ttl_zero();
    test_reentrant_subscribe_no_current_events();
    test_concurrent_publish_drain();
    return 0;
}
