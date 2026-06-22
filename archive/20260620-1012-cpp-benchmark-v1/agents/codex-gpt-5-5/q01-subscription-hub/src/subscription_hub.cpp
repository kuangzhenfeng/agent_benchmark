#include "subscription_hub.hpp"

#include <memory>
#include <utility>

SubscriptionHub::SubscriptionId SubscriptionHub::subscribe(std::string_view topic,
                                                           Callback callback,
                                                           std::uint32_t now_tick) {
    // Moving Callback into the separately-owned object can execute user code.
    // Do it before taking the hub lock.
    auto callback_owner = std::make_shared<Callback>(std::move(callback));
    std::string owned_topic(topic);
    SubscriptionId id;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        id = next_subscription_id_++;
        subscriptions_.emplace(
            id, Subscription{id, std::move(owned_topic), std::move(callback_owner), now_tick});
    }
    return id;
}

void SubscriptionHub::unsubscribe(SubscriptionId id) {
    // Keep the final Callback owner until after unlocking.  Its destructor is
    // user code for the purposes of this class and may re-enter the hub.
    std::shared_ptr<Callback> retired;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found = subscriptions_.find(id);
        if (found == subscriptions_.end()) {
            return;
        }
        retired = std::move(found->second.callback);
        subscriptions_.erase(found);
    }
}

void SubscriptionHub::publish(std::string_view topic, std::string_view payload) {
    // Event views are valid only for the callback.  Queue owned copies.
    QueuedEvent event{0, std::string(topic), std::string(payload)};
    std::lock_guard<std::mutex> lock(mutex_);
    event.sequence = next_sequence_++;
    pending_.push_back(std::move(event));
}

SubscriptionHub::DispatchStats SubscriptionHub::drain(std::uint32_t now_tick) {
    DispatchStats stats;
    std::deque<QueuedEvent> batch;
    SubscriptionId subscription_cutoff = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Detach exactly this batch before running callbacks.  Events
        // published by a callback are therefore available only to a later
        // drain, including a re-entrant one.
        batch.swap(pending_);
        subscription_cutoff = next_subscription_id_ - 1;
    }

    struct Delivery {
        SubscriptionId id;
        std::shared_ptr<Callback> callback;
    };

    for (const QueuedEvent& queued : batch) {
        const Event event{queued.sequence, queued.topic, queued.payload};
        std::vector<Delivery> deliveries;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& entry : subscriptions_) {
                const Subscription& subscription = entry.second;
                // IDs allocated after drain began do not participate in its
                // already-detached event batch.
                if (subscription.id <= subscription_cutoff &&
                    subscription.topic == event.topic) {
                    deliveries.push_back(Delivery{subscription.id, subscription.callback});
                }
            }
        }

        // A delivery list is a per-event snapshot: subscriptions removed by
        // another callback still receive this event if they were present when
        // delivery started, but no later event.  Each pointer keeps its
        // callable alive without copying that callable.
        for (const Delivery& delivery : deliveries) {
            try {
                (*delivery.callback)(event);
            } catch (...) {
                ++stats.callback_errors;
            }
            ++stats.delivered;

            // Activity is recorded after every attempted call, including an
            // exception.  A re-entrant unsubscribe may already have removed
            // the entry, in which case there is no live subscription to mark.
            {
                std::lock_guard<std::mutex> lock(mutex_);
                const auto found = subscriptions_.find(delivery.id);
                if (found != subscriptions_.end()) {
                    found->second.last_active_tick = now_tick;
                }
            }
        }
    }
    return stats;
}

std::size_t SubscriptionHub::expire_idle(std::uint32_t now_tick, std::uint32_t ttl) {
    std::vector<std::shared_ptr<Callback>> retired;
    std::size_t expired = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = subscriptions_.begin(); it != subscriptions_.end();) {
            const std::uint32_t elapsed = now_tick - it->second.last_active_tick;
            if (elapsed >= ttl) {
                retired.push_back(std::move(it->second.callback));
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
