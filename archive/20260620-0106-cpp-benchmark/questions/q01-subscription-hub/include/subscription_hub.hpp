#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
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
        SubscriptionId id;
        std::string_view topic;
        Callback callback;
        std::uint32_t last_active_tick;
    };

    struct QueuedEvent {
        std::uint64_t sequence;
        std::string_view topic;
        std::string_view payload;
    };

    mutable std::mutex mutex_;
    std::vector<Subscription> subscriptions_;
    std::deque<QueuedEvent> pending_;
    SubscriptionId next_subscription_id_ = 1;
    std::uint64_t next_sequence_ = 1;
};
