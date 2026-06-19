#include "subscription_hub.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {

// 32 位模运算下的“经过时间”。
// 在 ttl < 2^31 的前提下，无论是否发生回绕，delta >= ttl 即视为过期。
// 例如 now = 0xFFFFFFFF，last = 0x00000005，delta = 0xFFFFFFFA（视为 -11），
// 当 ttl 很大（但 < 2^31）时仍然 >= ttl，正确判定为过期。
std::uint32_t elapsed(std::uint32_t now, std::uint32_t last) {
    return now - last;
}

}  // namespace

SubscriptionHub::SubscriptionId SubscriptionHub::subscribe(std::string_view topic,
                                                           Callback callback,
                                                           std::uint32_t now_tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    const SubscriptionId id = next_subscription_id_++;
    auto data = std::make_shared<SubscriptionData>(id, std::string(topic),
                                                   std::move(callback), now_tick);
    subscriptions_.push_back(std::move(data));
    return id;
}

void SubscriptionHub::unsubscribe(SubscriptionId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    // 先从列表移除，再标记 alive=false。drain 在锁外投递时持有 shared_ptr，
    // 因此对象本身仍存活，但 alive=false 让它停止接收后续事件。
    // 对于正在投递当前事件的订阅者，遵循题意：取消只阻止“当前事件之后”的事件，
    // 因此不影响已经开始的这一轮投递（那一轮基于开始时的快照）。
    for (auto& sub : subscriptions_) {
        if (sub->id == id) {
            sub->alive.store(false, std::memory_order_release);
        }
    }
    subscriptions_.erase(
        std::remove_if(subscriptions_.begin(), subscriptions_.end(),
                       [id](const std::shared_ptr<SubscriptionData>& sub) {
                           return sub->id == id;
                       }),
        subscriptions_.end());
}

void SubscriptionHub::publish(std::string_view topic, std::string_view payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(QueuedEvent{next_sequence_++, std::string(topic),
                                   std::string(payload)});
}

SubscriptionHub::DispatchStats SubscriptionHub::drain(std::uint32_t now_tick) {
    // 1. 在锁内取出本批次的“在调用开始时已入队”事件快照，
    //    以及开始投递时仍处于订阅状态的订阅者快照。
    //    回调中新发布的事件留在 pending_，留给下一次 drain。
    std::vector<std::shared_ptr<SubscriptionData>> subscriptions_snapshot;
    std::vector<QueuedEvent> events;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        subscriptions_snapshot = subscriptions_;
        events.assign(pending_.begin(), pending_.end());
        pending_.clear();
    }

    DispatchStats stats;

    // 2. 完全在锁外投递，回调可重入 subscribe/unsubscribe/publish/drain。
    //    每个“开始投递时仍处于订阅状态”的订阅者至多收到每个事件一次。
    for (const QueuedEvent& queued : events) {
        // Event 的视图引用本次局部 queued 对象，在本轮回调返回前一直有效。
        const Event event{queued.sequence, queued.topic, queued.payload};
        for (const std::shared_ptr<SubscriptionData>& sub : subscriptions_snapshot) {
            if (!sub->alive.load(std::memory_order_acquire)) {
                // 取消订阅后不再接收后续事件；当前事件视作已被该订阅者尝试过。
                continue;
            }
            if (sub->topic != queued.topic) {
                continue;
            }
            // 单个回调抛异常不能阻止其余回调；该事件对该订阅者仍视为已处理，
            // 不会在下次 drain 重复投递（事件已离开队列）。
            // delivered 统计所有尝试调用次数（含抛异常），callback_errors 单独计数。
            try {
                sub->callback(event);
            } catch (...) {
                ++stats.callback_errors;
            }
            // 实际尝试回调后更新活跃 tick。
            sub->last_active_tick.store(now_tick, std::memory_order_release);
            ++stats.delivered;
        }
    }

    return stats;
}

std::size_t SubscriptionHub::expire_idle(std::uint32_t now_tick, std::uint32_t ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t expired = 0;
    std::vector<std::shared_ptr<SubscriptionData>> survivors;
    survivors.reserve(subscriptions_.size());
    for (auto& sub : subscriptions_) {
        const std::uint32_t last = sub->last_active_tick.load(std::memory_order_acquire);
        // ttl == 0 立即过期；否则经过时间 >= ttl 即过期（32 位模运算）。
        const bool is_expired = (ttl == 0) || (elapsed(now_tick, last) >= ttl);
        if (is_expired) {
            sub->alive.store(false, std::memory_order_release);
            ++expired;
        } else {
            survivors.push_back(std::move(sub));
        }
    }
    subscriptions_ = std::move(survivors);
    return expired;
}

std::size_t SubscriptionHub::pending_events() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_.size();
}
