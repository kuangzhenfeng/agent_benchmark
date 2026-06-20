#include "subscription_hub.hpp"

#include <algorithm>
#include <utility>

// 辅助函数：判断从 from 到 now 是否经过了至少 ttl 个 tick（32 位模运算）
static bool is_expired(std::uint32_t from, std::uint32_t now, std::uint32_t ttl) {
    if (ttl == 0) return true;
    std::uint32_t elapsed = now - from;
    return elapsed >= ttl;
}

SubscriptionHub::SubscriptionId SubscriptionHub::subscribe(std::string_view topic,
                                                           Callback callback,
                                                           std::uint32_t now_tick) {
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
                       [id](const Subscription& subscription) {
                           return subscription.id == id;
                       }),
        subscriptions_.end());
}

void SubscriptionHub::publish(std::string_view topic, std::string_view payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(
        QueuedEvent{next_sequence_++, std::string(topic), std::string(payload)});
}

SubscriptionHub::DispatchStats SubscriptionHub::drain(std::uint32_t now_tick) {
    // 1. 在锁内截取当前批次的事件和订阅者快照
    std::deque<QueuedEvent> events;
    std::vector<Subscription> subscriptions;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        events.swap(pending_);
        subscriptions = subscriptions_;
    }

    DispatchStats stats;

    // 2. 处理每个事件，回调在锁外执行
    for (const auto& queued : events) {
        Event event;
        event.sequence = queued.sequence;
        event.topic = queued.topic;
        event.payload = queued.payload;

        for (auto& subscription : subscriptions) {
            if (subscription.topic == event.topic) {
                try {
                    subscription.callback(event);
                } catch (...) {
                    ++stats.callback_errors;
                }
                subscription.last_active_tick = now_tick;
                ++stats.delivered;
            }
        }
    }

    // 3. 将更新后的活跃 tick 写回（订阅者可能已被移除，只更新仍存在的）
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& subscription : subscriptions) {
            auto it = std::find_if(subscriptions_.begin(), subscriptions_.end(),
                                   [id = subscription.id](const Subscription& s) {
                                       return s.id == id;
                                   });
            if (it != subscriptions_.end()) {
                it->last_active_tick = subscription.last_active_tick;
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
            return is_expired(subscription.last_active_tick, now_tick, ttl);
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
