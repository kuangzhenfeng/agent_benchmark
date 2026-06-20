#include "subscription_hub.hpp"

#include <algorithm>
#include <utility>

SubscriptionHub::SubscriptionId SubscriptionHub::subscribe(
    std::string_view topic, Callback callback, std::uint32_t now_tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    const SubscriptionId id = next_subscription_id_++;
    Subscription sub;
    sub.id = id;
    sub.owned_topic = std::string(topic);
    sub.callback = std::move(callback);
    sub.last_active_tick = now_tick;
    subscriptions_.push_back(std::move(sub));
    return id;
}

void SubscriptionHub::unsubscribe(SubscriptionId id) {
    std::vector<Subscription> keepers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        removed_.insert(id);
        auto it =
            std::partition(subscriptions_.begin(), subscriptions_.end(),
                           [id](const Subscription& s) { return s.id != id; });
        keepers.assign(std::make_move_iterator(subscriptions_.begin()),
                       std::make_move_iterator(it));
        subscriptions_.swap(keepers);
    }
}

void SubscriptionHub::publish(std::string_view topic, std::string_view payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(
        QueuedEvent{next_sequence_++, std::string(topic), std::string(payload)});
}

SubscriptionHub::DispatchStats SubscriptionHub::drain(std::uint32_t now_tick) {
    std::deque<QueuedEvent> events;
    std::vector<Subscription> subs_snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        events.swap(pending_);
        subs_snapshot = subscriptions_;
    }

    DispatchStats stats;
    std::unordered_set<SubscriptionId> delivered_for_event;

    for (const auto& ev : events) {
        delivered_for_event.clear();
        for (const auto& sub : subs_snapshot) {
            if (sub.owned_topic != ev.topic) continue;

            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (removed_.count(sub.id)) continue;
            }

            const Event event{ev.sequence, sub.owned_topic, ev.payload};
            try {
                sub.callback(event);
            } catch (...) {
                ++stats.delivered;
                ++stats.callback_errors;
                delivered_for_event.insert(sub.id);
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    for (auto& s : subscriptions_) {
                        if (s.id == sub.id) {
                            s.last_active_tick = now_tick;
                            break;
                        }
                    }
                }
                continue;
            }

            ++stats.delivered;
            delivered_for_event.insert(sub.id);

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
    std::vector<Subscription> expired;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto original_size = subscriptions_.size();
        auto it = std::partition(
            subscriptions_.begin(), subscriptions_.end(),
            [now_tick, ttl](const Subscription& s) {
                const std::uint32_t elapsed =
                    static_cast<std::uint32_t>(now_tick - s.last_active_tick);
                return elapsed < ttl;
            });
        expired.assign(std::make_move_iterator(it),
                       std::make_move_iterator(subscriptions_.end()));
        subscriptions_.erase(it, subscriptions_.end());
        return original_size - subscriptions_.size();
    }
}

std::size_t SubscriptionHub::pending_events() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_.size();
}
