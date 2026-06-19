#include "subscription_hub.hpp"

#include <atomic>
#include <cassert>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

int main() {
    {
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

    {
        SubscriptionHub hub;
        std::string tmp_topic = "temp_topic";
        std::string tmp_payload = "temp_payload";
        std::vector<std::string> got;
        SubscriptionHub::SubscriptionId sid = hub.subscribe(
            std::string_view(tmp_topic),
            [&got](const SubscriptionHub::Event& e) {
                got.emplace_back(e.payload);
            },
            1);
        tmp_topic.clear();
        tmp_payload.clear();

        std::string pub_topic = "temp_topic";
        std::string pub_payload = "hello";
        hub.publish(std::string_view(pub_topic), std::string_view(pub_payload));
        pub_topic.clear();
        pub_payload.clear();

        auto stats = hub.drain(2);
        assert(stats.delivered == 1);
        assert(got.size() == 1 && got[0] == "hello");
        (void)sid;
    }

    {
        SubscriptionHub hub;
        int count = 0;
        auto id = hub.subscribe("t", [&count, &hub](const SubscriptionHub::Event&) {
            ++count;
            hub.publish("t", "reentrant");
        }, 1);
        hub.publish("t", "first");
        auto stats = hub.drain(2);
        assert(stats.delivered == 1);
        assert(count == 1);
        assert(hub.pending_events() == 1);

        stats = hub.drain(3);
        assert(stats.delivered == 1);
        assert(count == 2);
        (void)id;
    }

    {
        SubscriptionHub hub;
        std::vector<std::string> got_a, got_b;
        auto id_a = hub.subscribe("t", [&got_a](const SubscriptionHub::Event& e) {
            got_a.emplace_back(e.payload);
        }, 1);
        auto id_b = hub.subscribe("t", [&got_b, &hub, id_a](const SubscriptionHub::Event& e) {
            got_b.emplace_back(e.payload);
            hub.unsubscribe(id_a);
        }, 1);

        hub.publish("t", "e1");
        hub.publish("t", "e2");
        auto stats = hub.drain(2);
        (void)id_b;
        assert(got_a.size() == 1);
        assert(got_b.size() == 2);
        assert(stats.delivered == 3);
    }

    {
        SubscriptionHub hub;
        int new_count = 0;
        hub.subscribe("t", [&hub, &new_count](const SubscriptionHub::Event&) {
            hub.subscribe("t", [&new_count](const SubscriptionHub::Event&) {
                ++new_count;
            }, 2);
        }, 1);
        hub.publish("t", "e1");
        hub.drain(2);
        assert(new_count == 0);
    }

    {
        SubscriptionHub hub;
        std::vector<int> results;
        hub.subscribe("t", [&results](const SubscriptionHub::Event&) {
            results.push_back(1);
        }, 1);
        hub.subscribe("t", [](const SubscriptionHub::Event&) {
            throw std::runtime_error("boom");
        }, 1);
        hub.subscribe("t", [&results](const SubscriptionHub::Event&) {
            results.push_back(3);
        }, 1);
        hub.publish("t", "e");
        auto stats = hub.drain(2);
        assert(stats.delivered == 3);
        assert(stats.callback_errors == 1);
        assert(results.size() == 2);
        assert(results[0] == 1 && results[1] == 3);
    }

    {
        SubscriptionHub hub;
        hub.subscribe("t", [](const SubscriptionHub::Event&) {}, 100);
        hub.publish("t", "x");
        hub.drain(200);

        const std::size_t expired = hub.expire_idle(240, 50);
        assert(expired == 0);

        const std::size_t expired2 = hub.expire_idle(250, 50);
        assert(expired2 == 1);
    }

    {
        SubscriptionHub hub;
        hub.subscribe("t", [](const SubscriptionHub::Event&) {}, 0xFFFFFF00u);

        const std::size_t n = hub.expire_idle(0x00000100u, 0x200);
        assert(n == 1);
    }

    {
        SubscriptionHub hub;
        hub.subscribe("t", [](const SubscriptionHub::Event&) {}, 100);
        const std::size_t n = hub.expire_idle(100, 0);
        assert(n == 1);
    }

    {
        SubscriptionHub hub;
        std::atomic<int> delivered_count{0};
        hub.subscribe("t", [&delivered_count](const SubscriptionHub::Event&) {
            ++delivered_count;
        }, 1);

        hub.publish("t", "e1");
        hub.publish("t", "e2");
        hub.publish("t", "e3");

        std::thread t1([&] { hub.drain(2); });
        std::thread t2([&] { hub.drain(3); });
        t1.join();
        t2.join();
        assert(delivered_count.load() == 3);
    }

    return 0;
}
