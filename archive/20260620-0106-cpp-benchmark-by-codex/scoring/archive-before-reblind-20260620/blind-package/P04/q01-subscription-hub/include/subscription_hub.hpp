#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

class SubscriptionHub {
public:
    using SubscriptionId = std::uint64_t;

    struct Event {
        std::uint64_t sequence = 0;
        std::string_view topic;
        std::string_view payload;
    };

    struct DispatchStats {
        std::size_t delivered = 0;
        std::size_t callback_errors = 0;
    };

    using Callback = std::function<void(const Event&)>;

    SubscriptionId subscribe(std::string_view topic, Callback callback,
                             std::uint32_t now_tick);
    void unsubscribe(SubscriptionId id);
    void publish(std::string_view topic, std::string_view payload);
    DispatchStats drain(std::uint32_t now_tick);
    std::size_t expire_idle(std::uint32_t now_tick, std::uint32_t ttl);
    std::size_t pending_events() const;

private:
    struct Subscription {
        SubscriptionId id = 0;
        std::string topic;
        Callback callback;
        std::uint32_t last_active_tick = 0;
        std::atomic<bool> active{true};
    };

    struct QueuedEvent {
        std::uint64_t sequence = 0;
        std::string topic;
        std::string payload;
    };

    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<Subscription>> subscriptions_;
    std::deque<QueuedEvent> pending_;
    SubscriptionId next_subscription_id_ = 1;
    std::uint64_t next_sequence_ = 1;
};
