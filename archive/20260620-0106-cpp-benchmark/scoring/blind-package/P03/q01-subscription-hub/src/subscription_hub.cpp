#include "subscription_hub.hpp"

#include <algorithm>
#include <utility>

SubscriptionHub::SubscriptionId SubscriptionHub::subscribe(std::string_view topic,
                                                           Callback callback,
                                                           std::uint32_t now_tick) {
    auto subscription = std::make_shared<Subscription>(Subscription{
        0, std::string(topic), std::move(callback), now_tick, true});
    std::lock_guard<std::mutex> lock(mutex_);
    const SubscriptionId id = next_subscription_id_++;
    subscription->id = id;
    subscriptions_.push_back(std::move(subscription));
    return id;
}

void SubscriptionHub::unsubscribe(SubscriptionId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& subscription : subscriptions_) {
        if (subscription->id == id) {
            subscription->active = false;
        }
    }
    subscriptions_.erase(
        std::remove_if(subscriptions_.begin(), subscriptions_.end(),
                       [id](const std::shared_ptr<Subscription>& subscription) {
                           return subscription->id == id;
                       }),
        subscriptions_.end());
}

void SubscriptionHub::publish(std::string_view topic, std::string_view payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(QueuedEvent{next_sequence_++, std::string(topic),
                                   std::string(payload)});
}

SubscriptionHub::DispatchStats SubscriptionHub::drain(std::uint32_t now_tick) {
    std::deque<QueuedEvent> batch;
    std::vector<std::shared_ptr<Subscription>> subscribers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Detaching the queue is the batch boundary.  Events published by a
        // callback (or another thread after this point) remain pending.
        batch.swap(pending_);
        subscribers = subscriptions_;
    }

    DispatchStats stats;
    for (const QueuedEvent& queued : batch) {
        const Event event{queued.sequence, queued.topic, queued.payload};
        for (const auto& subscription : subscribers) {
            bool should_invoke = false;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                // A snapshot makes new subscriptions ineligible for this
                // drain, while this check lets an unsubscribe stop later
                // events in the already detached batch.
                should_invoke = subscription->active &&
                                subscription->topic == event.topic;
            }

            if (!should_invoke) {
                continue;
            }

            ++stats.delivered;
            try {
                subscription->callback(event);
            } catch (...) {
                ++stats.callback_errors;
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (subscription->active) {
                    subscription->last_active_tick = now_tick;
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
        [now_tick, ttl](const std::shared_ptr<Subscription>& subscription) {
            const bool expired =
                static_cast<std::uint32_t>(now_tick - subscription->last_active_tick) >= ttl;
            if (expired) {
                subscription->active = false;
            }
            return expired;
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
