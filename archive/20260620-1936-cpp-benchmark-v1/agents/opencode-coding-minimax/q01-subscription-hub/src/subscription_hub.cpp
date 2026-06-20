#include "subscription_hub.hpp"

#include <algorithm>
#include <utility>

namespace {

bool is_expired(std::uint32_t now_tick, std::uint32_t last_active_tick,
                std::uint32_t ttl) {
    if (ttl == 0) {
        return true;
    }
    return (now_tick - last_active_tick) >= ttl;
}

}  // namespace

SubscriptionHub::SubscriptionId SubscriptionHub::subscribe(std::string_view topic,
                                                           Callback callback,
                                                           std::uint32_t now_tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    const SubscriptionId id = next_subscription_id_++;
    subscriptions_.push_back(Subscription{id, std::string(topic), std::move(callback), now_tick});
    return id;
}

void SubscriptionHub::unsubscribe(SubscriptionId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscriptions_.erase(
        std::remove_if(subscriptions_.begin(), subscriptions_.end(),
                       [id](const Subscription& subscription) {
                           return subscription.id == id;
                       }),
        subscriptions_.end());
}

void SubscriptionHub::publish(std::string_view topic, std::string_view payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(QueuedEvent{next_sequence_++, std::string(topic), std::string(payload)});
}

SubscriptionHub::DispatchStats SubscriptionHub::drain(std::uint32_t now_tick) {
    std::deque<QueuedEvent> snapshot;
    std::vector<Subscription> subscriptions_snapshot;
    std::vector<SubscriptionId> active_ids;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot = std::move(pending_);
        pending_.clear();

        for (auto& sub : subscriptions_) {
            subscriptions_snapshot.push_back(sub);
            active_ids.push_back(sub.id);
        }
    }

    DispatchStats stats;

    for (const QueuedEvent& queued : snapshot) {
        const Event event{queued.sequence, queued.topic, queued.payload};

        for (std::size_t i = 0; i < subscriptions_snapshot.size(); ++i) {
            const auto& subscription = subscriptions_snapshot[i];
            if (subscription.topic == event.topic) {
                bool still_active = false;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    for (const auto& sub : subscriptions_) {
                        if (sub.id == subscription.id) {
                            still_active = true;
                            break;
                        }
                    }
                }

                if (!still_active) {
                    continue;
                }

                try {
                    subscription.callback(event);
                } catch (...) {
                    ++stats.callback_errors;
                }
                ++stats.delivered;

                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    for (auto& sub : subscriptions_) {
                        if (sub.id == subscription.id) {
                            sub.last_active_tick = now_tick;
                            break;
                        }
                    }
                }
            }
        }
    }

    return stats;
}

std::size_t SubscriptionHub::expire_idle(std::uint32_t now_tick, std::uint32_t ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto old_end = std::remove_if(
        subscriptions_.begin(), subscriptions_.end(),
        [now_tick, ttl](const Subscription& subscription) {
            return is_expired(now_tick, subscription.last_active_tick, ttl);
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
