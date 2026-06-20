#include "subscription_hub.hpp"

#include <algorithm>
#include <utility>

SubscriptionHub::SubscriptionId SubscriptionHub::subscribe(std::string_view topic,
                                                           Callback callback,
                                                           std::uint32_t now_tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    const SubscriptionId id = next_subscription_id_++;
    subscriptions_.push_back(
        Subscription{id, std::string(topic), std::move(callback), now_tick});
    active_ids_.insert(id);
    return id;
}

void SubscriptionHub::unsubscribe(SubscriptionId id) {
    Callback dying;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        active_ids_.erase(id);
        auto it = std::find_if(subscriptions_.begin(), subscriptions_.end(),
                               [id](const Subscription& s) { return s.id == id; });
        if (it != subscriptions_.end()) {
            dying = std::move(it->callback);
            subscriptions_.erase(it);
        }
    }
}

void SubscriptionHub::publish(std::string_view topic, std::string_view payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(
        QueuedEvent{next_sequence_++, std::string(topic), std::string(payload)});
}

SubscriptionHub::DispatchStats SubscriptionHub::drain(std::uint32_t now_tick) {
    // 快照当前队列与订阅列表，清空队列（回调中发布的事件留给下次 drain）
    std::vector<QueuedEvent> events;
    std::vector<Subscription> subs;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        events.assign(pending_.begin(), pending_.end());
        pending_.clear();
        subs = subscriptions_;
    }

    DispatchStats stats;
    for (const QueuedEvent& qe : events) {
        for (Subscription& sub : subs) {
            if (sub.topic != qe.topic)
                continue;

            // 检查该订阅者是否仍在活跃（可能被回调中的 unsubscribe 移除）
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (active_ids_.find(sub.id) == active_ids_.end())
                    continue;
            }

            Event ev{qe.sequence, qe.topic, qe.payload};
            try {
                sub.callback(ev);
                ++stats.delivered;
            } catch (...) {
                ++stats.delivered;
                ++stats.callback_errors;
            }

            // 更新活跃 tick（若订阅在回调期间因重入被移除则跳过）
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = std::find_if(subscriptions_.begin(), subscriptions_.end(),
                                       [id = sub.id](const Subscription& s) {
                                           return s.id == id;
                                       });
                if (it != subscriptions_.end()) {
                    it->last_active_tick = now_tick;
                }
            }
        }
    }
    return stats;
}

std::size_t SubscriptionHub::expire_idle(std::uint32_t now_tick, std::uint32_t ttl) {
    std::vector<Callback> dying;
    std::size_t count = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscriptions_.begin();
        while (it != subscriptions_.end()) {
            // 32 位模运算：无符号减法自动处理回绕
            const std::uint32_t elapsed = now_tick - it->last_active_tick;
            if (elapsed >= ttl) {
                active_ids_.erase(it->id);
                dying.push_back(std::move(it->callback));
                it = subscriptions_.erase(it);
                ++count;
            } else {
                ++it;
            }
        }
    }
    return count;
}

std::size_t SubscriptionHub::pending_events() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_.size();
}