#include "subscription_hub.hpp"

#include <algorithm>
#include <utility>

SubscriptionHub::SubscriptionId SubscriptionHub::subscribe(std::string_view topic,
                                                            Callback callback,
                                                            std::uint32_t now_tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    const SubscriptionId id = next_subscription_id_++;
    subscriptions_.push_back(Subscription{id, std::string(topic), std::move(callback), now_tick});
    active_ids_.insert(id);
    return id;
}

void SubscriptionHub::unsubscribe(SubscriptionId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_ids_.erase(id);
    auto it = std::find_if(subscriptions_.begin(), subscriptions_.end(),
                           [id](const Subscription& s) { return s.id == id; });
    if (it != subscriptions_.end()) {
        subscriptions_.erase(it);
    }
}

void SubscriptionHub::publish(std::string_view topic, std::string_view payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(QueuedEvent{next_sequence_++, std::string(topic), std::string(payload)});
}

SubscriptionHub::DispatchStats SubscriptionHub::drain(std::uint32_t now_tick) {
    struct SubSnap {
        SubscriptionId id;
        std::string topic;
        Callback callback;
    };

    std::deque<QueuedEvent> events;
    std::vector<SubSnap> sub_snaps;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        events.swap(pending_);
        sub_snaps.reserve(subscriptions_.size());
        for (const auto& sub : subscriptions_) {
            sub_snaps.push_back(SubSnap{sub.id, sub.topic, sub.callback});
        }
    }

    DispatchStats stats;
    std::vector<SubscriptionId> attempted_ids;

    for (const auto& queued : events) {
        const Event event{queued.sequence, queued.topic, queued.payload};

        for (const auto& sub_snap : sub_snaps) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (active_ids_.find(sub_snap.id) == active_ids_.end()) {
                    continue;
                }
            }

            if (sub_snap.topic != event.topic) {
                continue;
            }

            try {
                sub_snap.callback(event);
            } catch (...) {
                ++stats.callback_errors;
            }
            ++stats.delivered;
            attempted_ids.push_back(sub_snap.id);
        }
    }

    if (!attempted_ids.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto id : attempted_ids) {
            for (auto& sub : subscriptions_) {
                if (sub.id == id) {
                    sub.last_active_tick = now_tick;
                    break;
                }
            }
        }
    }

    return stats;
}

std::size_t SubscriptionHub::expire_idle(std::uint32_t now_tick, std::uint32_t ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t expired = 0;
    for (auto it = subscriptions_.begin(); it != subscriptions_.end(); ) {
        const std::uint32_t elapsed = now_tick - it->last_active_tick;
        if (elapsed >= ttl) {
            active_ids_.erase(it->id);
            it = subscriptions_.erase(it);
            ++expired;
        } else {
            ++it;
        }
    }
    return expired;
}

std::size_t SubscriptionHub::pending_events() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_.size();
}
