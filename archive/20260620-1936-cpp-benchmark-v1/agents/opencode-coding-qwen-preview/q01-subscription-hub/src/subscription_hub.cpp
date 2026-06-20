#include "subscription_hub.hpp"

#include <algorithm>
#include <utility>

SubscriptionHub::SubscriptionId SubscriptionHub::subscribe(std::string_view topic,
                                                           Callback callback,
                                                           std::uint32_t now_tick) {
    auto cb_ptr = std::make_shared<Callback>(std::move(callback));
    std::lock_guard<std::mutex> lock(mutex_);
    const SubscriptionId id = next_subscription_id_++;
    subscriptions_.push_back(Subscription{id, std::string(topic), std::move(cb_ptr), now_tick});
    return id;
}

void SubscriptionHub::unsubscribe(SubscriptionId id) {
    std::shared_ptr<Callback> destroyed;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find_if(subscriptions_.begin(), subscriptions_.end(),
                               [id](const Subscription& s) { return s.id == id; });
        if (it != subscriptions_.end()) {
            destroyed = std::move(it->callback);
            subscriptions_.erase(it);
        }
    }
}

void SubscriptionHub::publish(std::string_view topic, std::string_view payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(QueuedEvent{next_sequence_++, std::string(topic), std::string(payload)});
}

SubscriptionHub::DispatchStats SubscriptionHub::drain(std::uint32_t now_tick) {
    std::deque<QueuedEvent> batch;
    std::unordered_set<SubscriptionId> initial_ids;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        batch.swap(pending_);
        initial_ids.reserve(subscriptions_.size());
        for (const auto& s : subscriptions_) {
            initial_ids.insert(s.id);
        }
    }

    DispatchStats stats;

    for (const auto& queued : batch) {
        const Event event{queued.sequence, queued.topic, queued.payload};

        struct SubSnap {
            SubscriptionId id;
            std::shared_ptr<Callback> cb;
        };
        std::vector<SubSnap> subs;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& s : subscriptions_) {
                if (initial_ids.count(s.id) && s.topic == event.topic) {
                    subs.push_back(SubSnap{s.id, s.callback});
                }
            }
        }

        std::vector<SubscriptionId> called;
        called.reserve(subs.size());
        for (const auto& sub : subs) {
            try {
                (*sub.cb)(event);
            } catch (...) {
                ++stats.callback_errors;
            }
            ++stats.delivered;
            called.push_back(sub.id);
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto uid : called) {
                auto it = std::find_if(subscriptions_.begin(), subscriptions_.end(),
                                       [uid](const Subscription& s) { return s.id == uid; });
                if (it != subscriptions_.end()) {
                    it->last_active_tick = now_tick;
                }
            }
        }
    }

    return stats;
}

std::size_t SubscriptionHub::expire_idle(std::uint32_t now_tick, std::uint32_t ttl) {
    std::vector<std::shared_ptr<Callback>> to_destroy;
    std::size_t expired = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscriptions_.begin();
        while (it != subscriptions_.end()) {
            const std::uint32_t elapsed = now_tick - it->last_active_tick;
            if (elapsed >= ttl) {
                to_destroy.push_back(std::move(it->callback));
                it = subscriptions_.erase(it);
                ++expired;
            } else {
                ++it;
            }
        }
    }
    return expired;
}

std::size_t SubscriptionHub::pending_events() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_.size();
}
