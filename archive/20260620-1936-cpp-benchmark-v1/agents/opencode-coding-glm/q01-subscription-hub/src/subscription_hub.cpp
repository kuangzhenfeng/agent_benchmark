#include "subscription_hub.hpp"

#include <algorithm>
#include <utility>

SubscriptionHub::SubscriptionId SubscriptionHub::subscribe(
    std::string_view topic, Callback callback, std::uint32_t now_tick) {
    std::string owned_topic(topic);
    auto cb_ptr = std::make_shared<Callback>(std::move(callback));
    std::lock_guard<std::mutex> lock(mutex_);
    const SubscriptionId id = next_subscription_id_++;
    subscriptions_.push_back(
        {id, std::move(owned_topic), std::move(cb_ptr), now_tick});
    return id;
}

void SubscriptionHub::unsubscribe(SubscriptionId id) {
    std::shared_ptr<Callback> cb_to_destroy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find_if(subscriptions_.begin(), subscriptions_.end(),
                               [id](const Subscription& s) { return s.id == id; });
        if (it != subscriptions_.end()) {
            cb_to_destroy = std::move(it->callback);
            subscriptions_.erase(it);
        }
    }
}

void SubscriptionHub::publish(std::string_view topic, std::string_view payload) {
    std::string owned_topic(topic);
    std::string owned_payload(payload);
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(
        {next_sequence_++, std::move(owned_topic), std::move(owned_payload)});
}

SubscriptionHub::DispatchStats SubscriptionHub::drain(std::uint32_t now_tick) {
    std::vector<QueuedEvent> events;
    SubscriptionId max_original_id = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        events.reserve(pending_.size());
        for (auto& qe : pending_) {
            events.emplace_back(std::move(qe));
        }
        pending_.clear();
        for (const auto& sub : subscriptions_) {
            if (sub.id > max_original_id) max_original_id = sub.id;
        }
    }

    DispatchStats stats;
    for (const auto& ev : events) {
        Event event{ev.sequence, ev.topic, ev.payload};

        std::vector<std::pair<SubscriptionId, std::shared_ptr<Callback>>> targets;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& sub : subscriptions_) {
                if (sub.id <= max_original_id && sub.topic == ev.topic) {
                    targets.emplace_back(sub.id, sub.callback);
                }
            }
        }

        for (const auto& [id, cb_ptr] : targets) {
            try {
                (*cb_ptr)(event);
            } catch (...) {
                ++stats.callback_errors;
            }
            ++stats.delivered;

            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& sub : subscriptions_) {
                    if (sub.id == id) {
                        sub.last_active_tick = now_tick;
                        break;
                    }
                }
            }
        }
    }
    return stats;
}

std::size_t SubscriptionHub::expire_idle(std::uint32_t now_tick, std::uint32_t ttl) {
    std::vector<std::shared_ptr<Callback>> cbs_to_destroy;
    std::size_t expired = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = subscriptions_.begin(); it != subscriptions_.end();) {
            if (static_cast<std::uint32_t>(now_tick - it->last_active_tick) >= ttl) {
                cbs_to_destroy.push_back(std::move(it->callback));
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
