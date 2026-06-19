#include "subscription_hub.hpp"

#include <algorithm>
#include <utility>

SubscriptionHub::SubscriptionId SubscriptionHub::subscribe(
    std::string_view topic, Callback callback, std::uint32_t now_tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    const SubscriptionId id = next_subscription_id_++;
    auto sub = std::make_shared<Subscription>();
    sub->id = id;
    sub->topic = std::string(topic);
    sub->callback = std::move(callback);
    sub->last_active_tick = now_tick;
    sub->active.store(true, std::memory_order_relaxed);
    subscriptions_.push_back(std::move(sub));
    return id;
}

void SubscriptionHub::unsubscribe(SubscriptionId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(subscriptions_.begin(), subscriptions_.end(),
                           [id](const std::shared_ptr<Subscription>& s) {
                               return s->id == id;
                           });
    if (it != subscriptions_.end()) {
        (*it)->active.store(false, std::memory_order_release);
        subscriptions_.erase(it);
    }
}

void SubscriptionHub::publish(std::string_view topic, std::string_view payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(QueuedEvent{next_sequence_++,
                                   std::string(topic),
                                   std::string(payload)});
}

SubscriptionHub::DispatchStats SubscriptionHub::drain(std::uint32_t now_tick) {
    std::deque<QueuedEvent> batch;
    std::vector<std::shared_ptr<Subscription>> subs_snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        batch.swap(pending_);
        subs_snapshot = subscriptions_;
    }

    DispatchStats stats;

    for (QueuedEvent& queued : batch) {
        std::vector<std::shared_ptr<Subscription>> targets;
        targets.reserve(subs_snapshot.size());
        for (const auto& sub : subs_snapshot) {
            if (sub->active.load(std::memory_order_acquire) &&
                sub->topic == queued.topic) {
                targets.push_back(sub);
            }
        }

        const Event event{queued.sequence,
                          std::string_view(queued.topic),
                          std::string_view(queued.payload)};

        for (const auto& sub : targets) {
            try {
                sub->callback(event);
            } catch (...) {
                ++stats.callback_errors;
            }
            ++stats.delivered;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                sub->last_active_tick = now_tick;
            }
        }
    }

    return stats;
}

std::size_t SubscriptionHub::expire_idle(std::uint32_t now_tick, std::uint32_t ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t expired = 0;
    auto is_expired = [now_tick, ttl](std::uint32_t last_active_tick) -> bool {
        if (ttl == 0) {
            return true;
        }
        const std::uint32_t elapsed = now_tick - last_active_tick;
        return elapsed >= ttl;
    };
    for (auto it = subscriptions_.begin(); it != subscriptions_.end();) {
        if (is_expired((*it)->last_active_tick)) {
            (*it)->active.store(false, std::memory_order_release);
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
