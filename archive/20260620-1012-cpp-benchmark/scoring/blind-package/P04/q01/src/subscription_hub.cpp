#include "subscription_hub.hpp"

#include <algorithm>
#include <utility>
#include <vector>

SubscriptionHub::SubscriptionId SubscriptionHub::subscribe(std::string_view topic,
                                                           Callback callback,
                                                           std::uint32_t now_tick) {
    // Construct the Subscription (which may move/copy the user callable) outside
    // the lock so user code never runs while mutex_ is held.
    auto sub = std::make_shared<Subscription>();
    sub->topic = std::string(topic);
    sub->callback = std::move(callback);
    sub->last_active_tick.store(now_tick);
    sub->alive.store(true);

    std::lock_guard<std::mutex> lock(mutex_);
    const SubscriptionId id = next_subscription_id_++;
    sub->id = id;
    subscriptions_.push_back(sub);  // shared_ptr copy: refcount bump only.
    return id;
}

void SubscriptionHub::unsubscribe(SubscriptionId id) {
    std::shared_ptr<Subscription> removed;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find_if(
            subscriptions_.begin(), subscriptions_.end(),
            [id](const std::shared_ptr<Subscription>& s) { return s->id == id; });
        if (it != subscriptions_.end()) {
            (*it)->alive.store(false, std::memory_order_relaxed);
            removed = std::move(*it);  // leaves *it null
            subscriptions_.erase(it);  // destroys a null shared_ptr: no user code
        }
    }
    // `removed` drops here, outside the lock. If this was the last owner the
    // Subscription (and its Callback) destruct now; user destructors may
    // re-enter public methods safely.
}

void SubscriptionHub::publish(std::string_view topic, std::string_view payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(QueuedEvent{next_sequence_++, std::string(topic),
                                   std::string(payload)});
}

SubscriptionHub::DispatchStats SubscriptionHub::drain(std::uint32_t now_tick) {
    std::vector<QueuedEvent> batch;
    std::vector<std::shared_ptr<Subscription>> subs_snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Snapshot exactly the events queued at drain start; new publishes by
        // callbacks land in the now-empty pending_ for the next drain.
        batch.assign(std::make_move_iterator(pending_.begin()),
                     std::make_move_iterator(pending_.end()));
        pending_.clear();
        // Snapshot subscribers present at drain start. New subscribers added by
        // callbacks are not in this snapshot and won't receive this batch.
        subs_snapshot = subscriptions_;
    }

    DispatchStats stats;
    for (const QueuedEvent& queued : batch) {
        const Event event{queued.sequence, queued.topic, queued.payload};
        for (const std::shared_ptr<Subscription>& sub : subs_snapshot) {
            // Skip subscribers that were unsubscribed/expired before this
            // delivery attempt. Within one event we still iterate the snapshot
            // in original order.
            if (!sub->alive.load(std::memory_order_relaxed)) continue;
            if (sub->topic != queued.topic) continue;
            try {
                sub->callback(event);
            } catch (...) {
                ++stats.callback_errors;
            }
            ++stats.delivered;  // counts every attempt, including throwing ones
            sub->last_active_tick.store(now_tick, std::memory_order_relaxed);
        }
    }
    return stats;
}

std::size_t SubscriptionHub::expire_idle(std::uint32_t now_tick, std::uint32_t ttl) {
    std::vector<std::shared_ptr<Subscription>> removed;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t write = 0;
        for (std::size_t read = 0; read < subscriptions_.size(); ++read) {
            auto& sub = subscriptions_[read];
            // 32-bit modular elapsed time. Given ttl < 2^31, this correctly
            // handles wraparound (e.g. last=0xFFFFFFFF, now=1 -> elapsed=2).
            const std::uint32_t elapsed =
                now_tick - sub->last_active_tick.load(std::memory_order_relaxed);
            if (elapsed >= ttl) {
                sub->alive.store(false, std::memory_order_relaxed);
                removed.push_back(std::move(sub));  // subscriptions_[read] now null
            } else {
                if (write != read) {
                    subscriptions_[write] = std::move(sub);
                }
                ++write;
            }
        }
        // Tail [write, size) holds moved-from (null) shared_ptrs.
        subscriptions_.resize(write);
    }
    // Removed callbacks destroy here, outside the lock.
    return removed.size();
}

std::size_t SubscriptionHub::pending_events() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_.size();
}
