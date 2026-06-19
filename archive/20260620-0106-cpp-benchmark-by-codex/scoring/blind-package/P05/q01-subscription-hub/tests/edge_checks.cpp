#include "subscription_hub.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

static void test_temporary_string_ownership() {
    SubscriptionHub hub;
    std::vector<std::string> received;
    hub.subscribe("topic", [&received](const SubscriptionHub::Event& event) {
        received.emplace_back(std::string(event.topic) + ":" + std::string(event.payload));
    }, 1);
    {
        std::string temp_topic("topic");
        std::string temp_payload("payload-from-temp");
        hub.publish(temp_topic, temp_payload);
    }
    const auto stats = hub.drain(2);
    assert(stats.delivered == 1);
    assert(stats.callback_errors == 0);
    assert(received.size() == 1);
    assert(received[0] == "topic:payload-from-temp");
}

static void test_reentrant_unsubscribe_blocks_future_events() {
    SubscriptionHub hub;
    std::vector<std::uint64_t> a_seqs, b_seqs;
    SubscriptionHub::SubscriptionId b_id = 0;
    b_id = hub.subscribe("t", [&b_seqs](const SubscriptionHub::Event& e) {
        b_seqs.push_back(e.sequence);
    }, 1);
    hub.subscribe("t", [&a_seqs, b_id, &hub](const SubscriptionHub::Event& e) {
        a_seqs.push_back(e.sequence);
        if (e.sequence == 1) {
            hub.unsubscribe(b_id);
        }
    }, 1);
    hub.publish("t", "e1");
    hub.publish("t", "e2");
    hub.publish("t", "e3");
    const auto stats = hub.drain(2);
    assert(stats.delivered == 4);
    assert(a_seqs.size() == 3);
    assert(b_seqs.size() == 1);
    assert(b_seqs[0] == 1);
}

static void test_reentrant_subscribe_does_not_receive_current_batch() {
    SubscriptionHub hub;
    std::vector<std::string> received;
    hub.subscribe("t", [&received, &hub](const SubscriptionHub::Event& e) {
        received.emplace_back(e.payload);
        if (e.sequence == 1) {
            hub.subscribe("t", [&received](const SubscriptionHub::Event& e2) {
                received.emplace_back(std::string("late:") + std::string(e2.payload));
            }, 5);
        }
    }, 1);
    hub.publish("t", "e1");
    hub.publish("t", "e2");
    const auto stats = hub.drain(2);
    assert(stats.delivered == 2);
    assert(received.size() == 2);
    assert(received[0] == "e1");
    assert(received[1] == "e2");
    assert(hub.pending_events() == 0);

    hub.publish("t", "e3");
    const auto stats2 = hub.drain(3);
    assert(stats2.delivered == 2);
    assert(received.size() == 4);
    assert(received[2] == "e3");
    assert(received[3] == "late:e3");
}

static void test_reentrant_publish_deferred_to_next_drain() {
    SubscriptionHub hub;
    std::vector<std::string> received;
    hub.subscribe("t", [&received, &hub](const SubscriptionHub::Event& e) {
        received.emplace_back(e.payload);
        if (e.sequence == 1) {
            hub.publish("t", "late");
        }
    }, 1);
    hub.publish("t", "first");
    const auto stats = hub.drain(2);
    assert(stats.delivered == 1);
    assert(received.size() == 1);
    assert(received[0] == "first");
    assert(hub.pending_events() == 1);

    const auto stats2 = hub.drain(3);
    assert(stats2.delivered == 1);
    assert(received.size() == 2);
    assert(received[1] == "late");
}

static void test_recursive_drain_processes_newly_published() {
    SubscriptionHub hub;
    std::vector<std::string> received;
    hub.subscribe("t", [&received, &hub](const SubscriptionHub::Event& e) {
        received.emplace_back(e.payload);
        if (e.sequence == 1) {
            hub.publish("t", "during");
            hub.drain(2);
        }
    }, 1);
    hub.publish("t", "first");
    const auto stats = hub.drain(1);
    assert(stats.delivered == 1);
    assert(received.size() == 2);
    assert(received[0] == "first");
    assert(received[1] == "during");
    assert(hub.pending_events() == 0);
}

static void test_callback_exception_does_not_block_others() {
    SubscriptionHub hub;
    std::vector<std::string> received;
    hub.subscribe("t", [](const SubscriptionHub::Event&) {
        throw std::runtime_error("boom");
    }, 1);
    hub.subscribe("t", [&received](const SubscriptionHub::Event& e) {
        received.emplace_back(e.payload);
    }, 1);
    hub.publish("t", "e1");
    hub.publish("t", "e2");
    const auto stats = hub.drain(2);
    assert(stats.delivered == 4);
    assert(stats.callback_errors == 2);
    assert(received.size() == 2);
}

static void test_no_redelivery_after_exception() {
    SubscriptionHub hub;
    std::vector<std::uint64_t> thrower_seen;
    hub.subscribe("t", [&thrower_seen](const SubscriptionHub::Event& e) {
        thrower_seen.push_back(e.sequence);
        throw std::runtime_error("always");
    }, 1);
    hub.publish("t", "e1");
    hub.publish("t", "e2");
    const auto stats = hub.drain(2);
    assert(stats.delivered == 2);
    assert(stats.callback_errors == 2);
    assert(thrower_seen.size() == 2);

    const auto stats2 = hub.drain(3);
    assert(stats2.delivered == 0);
    assert(thrower_seen.size() == 2);
}

static void test_ttl_zero_immediate_expiry() {
    SubscriptionHub hub;
    hub.subscribe("t", [](const SubscriptionHub::Event&) {}, 100);
    const std::size_t expired = hub.expire_idle(100, 0);
    assert(expired == 1);
}

static void test_ttl_boundary_not_expired() {
    SubscriptionHub hub;
    hub.subscribe("t", [](const SubscriptionHub::Event&) {}, 100);
    const std::size_t expired = hub.expire_idle(109, 10);
    assert(expired == 0);
    const std::size_t expired2 = hub.expire_idle(110, 10);
    assert(expired2 == 1);
}

static void test_active_tick_refreshed_after_callback() {
    SubscriptionHub hub;
    hub.subscribe("t", [](const SubscriptionHub::Event&) {}, 100);
    hub.publish("t", "x");
    hub.drain(150);
    const std::size_t expired = hub.expire_idle(159, 10);
    assert(expired == 0);
    const std::size_t expired2 = hub.expire_idle(160, 10);
    assert(expired2 == 1);
}

static void test_wraparound_expiry() {
    SubscriptionHub hub;
    hub.subscribe("t", [](const SubscriptionHub::Event&) {}, 0xFFFFFFF0u);
    const std::size_t expired_short = hub.expire_idle(0xFFFFFFF5u, 10);
    assert(expired_short == 0);
    const std::size_t expired_ok = hub.expire_idle(84u, 100);
    assert(expired_ok == 1);
}

static void test_wraparound_not_expired_when_recent() {
    SubscriptionHub hub;
    hub.subscribe("t", [](const SubscriptionHub::Event&) {}, 0xFFFFFFF0u);
    const std::size_t expired = hub.expire_idle(5u, 1000);
    assert(expired == 0);
}

static void test_concurrent_publish_drain() {
    SubscriptionHub hub;
    std::atomic<std::size_t> delivered_total{0};
    hub.subscribe("t", [&delivered_total](const SubscriptionHub::Event&) {
        delivered_total.fetch_add(1, std::memory_order_relaxed);
    }, 1);

    const int publish_threads = 4;
    const int drain_threads = 2;
    const int events_per_publisher = 500;
    const std::size_t expected =
        static_cast<std::size_t>(publish_threads) * events_per_publisher;

    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    for (int i = 0; i < publish_threads; ++i) {
        threads.emplace_back([&hub, events_per_publisher, i]() {
            for (int j = 0; j < events_per_publisher; ++j) {
                hub.publish("t", std::to_string(i) + ":" + std::to_string(j));
            }
        });
    }
    for (int i = 0; i < drain_threads; ++i) {
        threads.emplace_back([&hub, &stop]() {
            while (!stop.load(std::memory_order_relaxed)) {
                hub.drain(1);
            }
            hub.drain(1);
        });
    }

    for (int i = 0; i < publish_threads; ++i) {
        threads[i].join();
    }
    stop.store(true, std::memory_order_relaxed);
    for (int i = publish_threads; i < publish_threads + drain_threads; ++i) {
        threads[i].join();
    }

    for (int i = 0; i < 100 && hub.pending_events() > 0; ++i) {
        hub.drain(1);
    }
    assert(hub.pending_events() == 0);
    assert(delivered_total.load() == expected);
}

int main() {
    test_temporary_string_ownership();
    test_reentrant_unsubscribe_blocks_future_events();
    test_reentrant_subscribe_does_not_receive_current_batch();
    test_reentrant_publish_deferred_to_next_drain();
    test_recursive_drain_processes_newly_published();
    test_callback_exception_does_not_block_others();
    test_no_redelivery_after_exception();
    test_ttl_zero_immediate_expiry();
    test_ttl_boundary_not_expired();
    test_active_tick_refreshed_after_callback();
    test_wraparound_expiry();
    test_wraparound_not_expired_when_recent();
    test_concurrent_publish_drain();
    return 0;
}
