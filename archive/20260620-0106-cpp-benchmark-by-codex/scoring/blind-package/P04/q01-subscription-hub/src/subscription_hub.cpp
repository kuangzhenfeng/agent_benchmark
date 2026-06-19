#include "subscription_hub.hpp"

#include <algorithm>
#include <utility>

SubscriptionHub::SubscriptionId SubscriptionHub::subscribe(
    std::string_view topic, Callback callback, std::uint32_t now_tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    const SubscriptionId id = next_subscription_id_++;
    subscriptions_.push_back(
        Subscription{id, std::string(topic), std::move(callback), now_tick});
    return id;
}

void SubscriptionHub::unsubscribe(SubscriptionId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscriptions_.erase(
        std::remove_if(subscriptions_.begin(), subscriptions_.end(),
                       [id](const Subscription& s) { return s.id == id; }),
        subscriptions_.end());
}

void SubscriptionHub::publish(std::string_view topic,
                               std::string_view payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(QueuedEvent{next_sequence_++, std::string(topic),
                                   std::string(payload)});
}

SubscriptionHub::DispatchStats SubscriptionHub::drain(std::uint32_t now_tick) {
    // Step 1: snapshot pending events and subscriptions under lock
    std::deque<QueuedEvent> batch;
    std::vector<Subscription> subs_snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        batch.swap(pending_);
        subs_snapshot = subscriptions_;
    }

    DispatchStats stats;

    for (const auto& queued : batch) {
        // Event views point into the QueuedEvent's owned strings,
        // which live for the entire drain scope.
        const Event event{queued.sequence, queued.topic, queued.payload};

        for (const auto& sub : subs_snapshot) {
            if (sub.topic != queued.topic) continue;

            // Liveness check: skip if unsubscribed since snapshot
            {
                std::lock_guard<std::mutex> lock(mutex_);
                const bool alive = std::any_of(
                    subscriptions_.begin(), subscriptions_.end(),
                    [&sub](const Subscription& s) {
                        return s.id == sub.id;
                    });
                if (!alive) continue;
            }

            // Invoke callback WITHOUT holding the lock
            try {
                sub.callback(event);
            } catch (...) {
                ++stats.callback_errors;
            }
            ++stats.delivered;

            // Update last_active_tick (spec: after each callback attempt)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& s : subscriptions_) {
                    if (s.id == sub.id) {
                        s.last_active_tick = now_tick;
                        break;
                    }
                }
            }
        }
    }

    return stats;
}

std::size_t SubscriptionHub::expire_idle(std::uint32_t now_tick,
                                          std::uint32_t ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Unsigned modular subtraction handles uint32_t wraparound correctly
    // when ttl < 2^31. TTL=0: elapsed >= 0 is always true → immediate expire.
    const auto old_end = std::remove_if(
        subscriptions_.begin(), subscriptions_.end(),
        [now_tick, ttl](const Subscription& sub) {
            const std::uint32_t elapsed =
                now_tick - sub.last_active_tick;
            return elapsed >= ttl;
        });
    const std::size_t expired =
        static_cast<std::size_t>(std::distance(old_end, subscriptions_.end()));
    subscriptions_.erase(old_end, subscriptions_.end());
    return expired;
}

std::size_t SubscriptionHub::pending_events() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_.size();
}
