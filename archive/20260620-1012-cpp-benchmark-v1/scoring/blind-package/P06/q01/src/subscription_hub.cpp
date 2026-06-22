#include "subscription_hub.hpp"

#include <memory>
#include <utility>

namespace {

bool is_expired(std::uint32_t now_tick, std::uint32_t last_active_tick,
                std::uint32_t ttl) {
    if (ttl == 0) {
        return true;
    }
    return static_cast<std::uint32_t>(now_tick - last_active_tick) >= ttl;
}

}  // namespace

SubscriptionHub::SubscriptionId SubscriptionHub::subscribe(std::string_view topic,
                                                           Callback callback,
                                                           std::uint32_t now_tick) {
    auto callback_slot =
        std::make_shared<CallbackSlot>(CallbackSlot{std::move(callback)});

    std::lock_guard<std::mutex> lock(mutex_);
    const SubscriptionId id = next_subscription_id_++;
    subscriptions_.push_back(
        Subscription{id, std::string(topic), std::move(callback_slot), now_tick});
    return id;
}

void SubscriptionHub::unsubscribe(SubscriptionId id) {
    std::vector<std::shared_ptr<CallbackSlot>> retired_callbacks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto write = subscriptions_.begin();
        for (auto it = subscriptions_.begin(); it != subscriptions_.end(); ++it) {
            if (it->id == id) {
                retired_callbacks.push_back(std::move(it->callback));
                continue;
            }
            if (write != it) {
                *write = std::move(*it);
            }
            ++write;
        }
        subscriptions_.erase(write, subscriptions_.end());
    }
}

void SubscriptionHub::publish(std::string_view topic, std::string_view payload) {
    QueuedEvent event{0, std::string(topic), std::string(payload)};

    std::lock_guard<std::mutex> lock(mutex_);
    event.sequence = next_sequence_++;
    pending_.push_back(std::move(event));
}

SubscriptionHub::DispatchStats SubscriptionHub::drain(std::uint32_t now_tick) {
    std::deque<QueuedEvent> batch;
    SubscriptionId batch_subscription_cutoff = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        batch_subscription_cutoff = next_subscription_id_;
        batch.swap(pending_);
    }

    DispatchStats stats;
    for (const QueuedEvent& queued : batch) {
        std::vector<std::pair<SubscriptionId, std::shared_ptr<CallbackSlot>>> recipients;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            recipients.reserve(subscriptions_.size());
            for (const Subscription& subscription : subscriptions_) {
                // 只向本次批次开始前已存在的订阅者分发，当前事件的接收者集合按事件开始时快照。
                if (subscription.id < batch_subscription_cutoff &&
                    subscription.topic == queued.topic) {
                    recipients.emplace_back(subscription.id, subscription.callback);
                }
            }
        }

        const Event event{queued.sequence, queued.topic, queued.payload};
        for (const auto& recipient : recipients) {
            try {
                recipient.second->callback(event);
            } catch (...) {
                ++stats.callback_errors;
            }
            ++stats.delivered;

            std::lock_guard<std::mutex> lock(mutex_);
            for (Subscription& subscription : subscriptions_) {
                if (subscription.id == recipient.first) {
                    subscription.last_active_tick = now_tick;
                    break;
                }
            }
        }
    }

    return stats;
}

std::size_t SubscriptionHub::expire_idle(std::uint32_t now_tick, std::uint32_t ttl) {
    std::vector<std::shared_ptr<CallbackSlot>> retired_callbacks;
    std::size_t expired = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto write = subscriptions_.begin();
        for (auto it = subscriptions_.begin(); it != subscriptions_.end(); ++it) {
            if (is_expired(now_tick, it->last_active_tick, ttl)) {
                retired_callbacks.push_back(std::move(it->callback));
                ++expired;
                continue;
            }
            if (write != it) {
                *write = std::move(*it);
            }
            ++write;
        }
        subscriptions_.erase(write, subscriptions_.end());
    }
    return expired;
}

std::size_t SubscriptionHub::pending_events() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_.size();
}
