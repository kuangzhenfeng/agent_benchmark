#include "subscription_hub.hpp"

#include <algorithm>
#include <utility>

SubscriptionHub::SubscriptionId SubscriptionHub::subscribe(
    std::string_view topic, Callback callback, std::uint32_t now_tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    const SubscriptionId id = next_subscription_id_++;
    subscriptions_.push_back(
        Subscription{id, std::string(topic),
                     std::make_shared<Callback>(std::move(callback)),
                     now_tick});
    return id;
}

void SubscriptionHub::unsubscribe(SubscriptionId id) {
    // Move the removed subscription out so the callback (and its user callable)
    // is destroyed outside the lock — satisfying the callable-lifetime rule.
    Subscription removed{};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find_if(subscriptions_.begin(), subscriptions_.end(),
                               [id](const Subscription& s) {
                                   return s.id == id;
                               });
        if (it != subscriptions_.end()) {
            removed = std::move(*it);
            subscriptions_.erase(it);
        }
    }
    // `removed` and its shared_ptr<Callback> are destroyed here, outside lock.
}

void SubscriptionHub::publish(std::string_view topic, std::string_view payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(
        QueuedEvent{next_sequence_++, std::string(topic), std::string(payload)});
}

SubscriptionHub::DispatchStats SubscriptionHub::drain(std::uint32_t now_tick) {
    // Phase 1: capture batch and subscription snapshot under lock.
    std::deque<QueuedEvent> batch;
    std::vector<Subscription> drain_subs;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        batch.swap(pending_);
        drain_subs = subscriptions_;  // copies shared_ptrs (refcount++)
    }

    DispatchStats stats;

    // Phase 2: deliver events outside lock.
    for (const QueuedEvent& queued : batch) {
        const Event event{queued.sequence, queued.topic, queued.payload};
        for (const Subscription& sub : drain_subs) {
            if (sub.topic != queued.topic) {
                continue;
            }

            // Check if still subscribed (might have been removed by an
            // earlier callback in this batch).
            bool still_active = false;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                still_active = std::any_of(
                    subscriptions_.begin(), subscriptions_.end(),
                    [&sub](const Subscription& s) {
                        return s.id == sub.id;
                    });
            }
            if (!still_active) {
                continue;
            }

            // Invoke callback outside lock.
            ++stats.delivered;
            try {
                (*sub.callback)(event);
            } catch (...) {
                ++stats.callback_errors;
            }

            // Update last_active_tick under lock (if still present).
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
    // drain_subs destroyed here — callback shared_ptrs released outside lock.
}

std::size_t SubscriptionHub::expire_idle(std::uint32_t now_tick,
                                          std::uint32_t ttl) {
    // Collect expired subscriptions under lock, destroy their callbacks outside.
    std::vector<Subscription> expired;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::partition(
            subscriptions_.begin(), subscriptions_.end(),
            [now_tick, ttl](const Subscription& s) {
                // Modular elapsed time — correct for 32-bit wraparound
                // when ttl < 2^31.
                const std::uint32_t elapsed = now_tick - s.last_active_tick;
                // elapsed >= ttl means expired (keep = false).
                // TTL=0: elapsed >= 0 is always true → all expired.
                return elapsed < ttl;
            });
        expired.assign(std::make_move_iterator(it),
                       std::make_move_iterator(subscriptions_.end()));
        subscriptions_.erase(it, subscriptions_.end());
    }
    const std::size_t count = expired.size();
    // `expired` destroyed here — callbacks released outside lock.
    return count;
}

std::size_t SubscriptionHub::pending_events() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_.size();
}
