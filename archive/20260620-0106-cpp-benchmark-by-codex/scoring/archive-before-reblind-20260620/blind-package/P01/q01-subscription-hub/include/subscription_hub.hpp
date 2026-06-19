#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

class SubscriptionHub {
public:
    using SubscriptionId = std::uint64_t;

    struct Event {
        std::uint64_t sequence = 0;
        std::string_view topic;
        std::string_view payload;
    };

    struct DispatchStats {
        std::size_t delivered = 0;
        std::size_t callback_errors = 0;
    };

    using Callback = std::function<void(const Event&)>;

    SubscriptionId subscribe(std::string_view topic, Callback callback,
                             std::uint32_t now_tick);
    void unsubscribe(SubscriptionId id);
    void publish(std::string_view topic, std::string_view payload);
    DispatchStats drain(std::uint32_t now_tick);
    std::size_t expire_idle(std::uint32_t now_tick, std::uint32_t ttl);
    std::size_t pending_events() const;

private:
    // 订阅记录使用引用计数的共享对象，使得 drain 在锁外投递期间，
    // 即使 unsubscribe 已经把它从 subscriptions_ 中移除，回调指针依旧有效。
    struct SubscriptionData {
        SubscriptionId id;
        std::string topic;          // 自有存储，避免 string_view 悬空
        Callback callback;
        std::atomic<std::uint32_t> last_active_tick;
        std::atomic<bool> alive;    // 取消订阅/过期后置 false，锁外投递据此跳过

        SubscriptionData(SubscriptionId id_, std::string topic_, Callback cb,
                         std::uint32_t tick)
            : id(id_), topic(std::move(topic_)), callback(std::move(cb)),
              last_active_tick(tick), alive(true) {}
    };

    struct QueuedEvent {
        std::uint64_t sequence;
        std::string topic;          // 自有存储
        std::string payload;        // 自有存储
    };

    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<SubscriptionData>> subscriptions_;
    std::deque<QueuedEvent> pending_;
    SubscriptionId next_subscription_id_ = 1;
    std::uint64_t next_sequence_ = 1;
};
