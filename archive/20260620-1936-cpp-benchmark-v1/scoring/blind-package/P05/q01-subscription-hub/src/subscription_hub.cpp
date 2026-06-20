#include "subscription_hub.hpp"

#include <algorithm>
#include <utility>

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
    // 1. 锁下快照当前 pending 和订阅者，然后解锁
    struct DrainSnapshot {
        std::deque<QueuedEvent> events;
        struct SubscriberInfo {
            SubscriptionId id;
            std::string topic;
            Callback callback;
        };
        std::vector<SubscriberInfo> subscribers;
    };

    DrainSnapshot snapshot;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        in_drain_ = true;
        snapshot.events = std::move(pending_);
        pending_.clear();

        // 保存订阅者快照（含 callback），之后在锁外调用
        for (auto& sub : subscriptions_) {
            snapshot.subscribers.push_back(
                DrainSnapshot::SubscriberInfo{sub.id, sub.topic, sub.callback});
        }
    }

    // 2. 锁外执行回调，处理异常和重入
    DispatchStats stats;

    // 记录每个事件已尝试调用的订阅者 ID（用于防止重复投递）
    // 以及统计 delivered/callback_errors
    for (const auto& event : snapshot.events) {
        const Event evt{event.sequence, event.topic, event.payload};

        // 收集当前事件仍活跃的订阅者（在锁下检查）
        std::vector<SubscriptionId> active_ids;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& sub : subscriptions_) {
                if (sub.topic == event.topic) {
                    active_ids.push_back(sub.id);
                }
            }
        }

        // 为每个活跃订阅者找回调并调用（锁外）
        for (const auto& sub_info : snapshot.subscribers) {
            // 检查该订阅者是否仍活跃且匹配 topic
            bool is_active = false;
            for (auto aid : active_ids) {
                if (aid == sub_info.id) {
                    is_active = true;
                    break;
                }
            }
            if (!is_active) continue;

            ++stats.delivered;
            try {
                sub_info.callback(evt);
            } catch (...) {
                ++stats.callback_errors;
            }

            // 回调后更新 last_active_tick（锁下）
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& sub : subscriptions_) {
                    if (sub.id == sub_info.id) {
                        sub.last_active_tick = now_tick;
                        break;
                    }
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        in_drain_ = false;
    }

    return stats;
}

std::size_t SubscriptionHub::expire_idle(std::uint32_t now_tick, std::uint32_t ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto old_end = std::remove_if(
        subscriptions_.begin(), subscriptions_.end(),
        [now_tick, ttl](const Subscription& subscription) {
            if (ttl == 0) return true;
            // 32 位模运算：使用减法判断经过时间
            const std::uint32_t elapsed = now_tick - subscription.last_active_tick;
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
