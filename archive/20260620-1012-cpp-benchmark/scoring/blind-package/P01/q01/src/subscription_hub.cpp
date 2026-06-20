#include "subscription_hub.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace {

// 32-bit modular elapsed time. For ttl < 2^31, (now - last) >= ttl is the
// correct expiry test even across uint32_t wrap-around.
bool is_expired(std::uint32_t now_tick, std::uint32_t last_active,
                std::uint32_t ttl) {
    if (ttl == 0) {
        return true;
    }
    const std::uint32_t elapsed = now_tick - last_active;  // modular subtract
    return elapsed >= ttl;
}

}  // namespace

SubscriptionHub::SubscriptionId SubscriptionHub::subscribe(
    std::string_view topic, Callback callback, std::uint32_t now_tick) {
    // Build the subscription (including moving/copying the user callable, which
    // may run user code) BEFORE acquiring the lock.
    auto subscription = std::make_shared<Subscription>();
    subscription->topic = std::string(topic);  // own the (possibly temp) topic
    subscription->callback = std::move(callback);
    subscription->last_active_tick.store(now_tick, std::memory_order_relaxed);
    subscription->cancelled.store(false, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(mutex_);
    const SubscriptionId id = next_subscription_id_++;
    subscription->id = id;
    // Pushing a shared_ptr only moves the control block pointer — no user code.
    subscriptions_.push_back(std::move(subscription));
    return id;
}

void SubscriptionHub::unsubscribe(SubscriptionId id) {
    std::shared_ptr<Subscription> removed;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find_if(
            subscriptions_.begin(), subscriptions_.end(),
            [id](const std::shared_ptr<Subscription>& subscription) {
                return subscription->id == id;
            });
        if (it != subscriptions_.end()) {
            removed = std::move(*it);        // move control-block ptr; no user code
            subscriptions_.erase(it);        // slot now holds a null shared_ptr
        }
    }
    // Dropping the final owner (and thus the user callable's destructor, which
    // may re-enter the hub) happens here, outside the lock.
    removed.reset();
}

void SubscriptionHub::publish(std::string_view topic, std::string_view payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    QueuedEvent event;
    event.sequence = next_sequence_++;
    event.topic = std::string(topic);      // own topic storage (input may be temp)
    event.payload = std::string(payload);  // own payload storage (input may be temp)
    pending_.push_back(std::move(event));
}

SubscriptionHub::DispatchStats SubscriptionHub::drain(std::uint32_t now_tick) {
    // 1. Capture exactly the events queued at drain start, and the subscribers
    //    subscribed at drain start. Both snapshots are taken under the lock and
    //    then the lock is released for the entire delivery phase — callbacks may
    //    re-enter subscribe/unsubscribe/publish/drain without deadlocking.
    std::deque<QueuedEvent> batch;
    std::vector<std::shared_ptr<Subscription>> audience;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        batch.swap(pending_);  // new events published during delivery go to a
                               // fresh pending_ and are left for the next drain
        for (auto& subscription : subscriptions_) {
            if (!subscription->cancelled.load(std::memory_order_relaxed)) {
                audience.push_back(subscription);  // shared_ptr copy keeps them alive
            }
        }
    }

    DispatchStats stats;

    // 2. Deliver. `audience` is fixed at drain start, so subscriptions created
    //    during a callback never receive any event from this batch. Cancellation
    //    is observed per-event via the atomic flag, so a subscriber cancelled
    //    before an event's delivery start does not receive that or later events.
    for (const QueuedEvent& queued : batch) {
        const Event event{queued.sequence, queued.topic, queued.payload};
        for (auto& target : audience) {
            if (target->cancelled.load(std::memory_order_relaxed)) {
                continue;  // unsubscribed before this event's delivery start
            }
            bool threw = false;
            try {
                target->callback(event);
            } catch (...) {
                threw = true;
            }
            // Update active tick after every actual callback attempt, including
            // ones that threw.
            target->last_active_tick.store(now_tick, std::memory_order_relaxed);
            ++stats.delivered;            // counts every attempt (incl. throwing)
            if (threw) {
                ++stats.callback_errors;  // exceptions counted separately
            }
        }
    }

    return stats;
}

std::size_t SubscriptionHub::expire_idle(std::uint32_t now_tick,
                                         std::uint32_t ttl) {
    std::vector<std::shared_ptr<Subscription>> removed;
    std::size_t expired = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::remove_if(
            subscriptions_.begin(), subscriptions_.end(),
            [&](const std::shared_ptr<Subscription>& subscription) {
                const std::uint32_t last =
                    subscription->last_active_tick.load(std::memory_order_relaxed);
                if (is_expired(now_tick, last, ttl)) {
                    removed.push_back(subscription);  // keep alive until release
                    return true;
                }
                return false;
            });
        expired = static_cast<std::size_t>(std::distance(it, subscriptions_.end()));
        subscriptions_.erase(it, subscriptions_.end());
    }
    // User callable destructors run here, outside the lock.
    removed.clear();
    return expired;
}

std::size_t SubscriptionHub::pending_events() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_.size();
}
