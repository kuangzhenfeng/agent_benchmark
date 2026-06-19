#include "coalescing_cache.hpp"

#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

LoadResult LoadResult::ok(std::string value) {
    return LoadResult{LoadStatus::ok, std::move(value), {}};
}

LoadResult LoadResult::failed(std::string error) {
    return LoadResult{LoadStatus::loader_failed, {}, std::move(error)};
}

LoadResult LoadResult::recursive() {
    return LoadResult{LoadStatus::recursive_load, {}, "recursive load"};
}

struct CoalescingCache::State {
    explicit State(std::size_t cap, Clock clock)
        : capacity(cap), now(std::move(clock)) {}

    // ready cache 的一个条目：值 + 过期时刻 + 在 LRU 链表中的位置。
    struct ReadyNode {
        std::string value;
        std::uint64_t expire_time = 0;
        std::list<std::string>::iterator lru_pos;
    };

    // 某个 key 的在途加载。loader 与所有等待者共享同一个 slot。
    struct InFlight {
        std::mutex m;
        std::condition_variable cv;
        bool completed = false;
        bool discard_result = false;  // invalidate 期间设置：loader 完成后不写回 ready
        LoadResult result{LoadStatus::loader_failed, {}, {}};
        std::thread::id loader_thread;
    };

    std::size_t capacity;
    Clock now;
    mutable std::mutex mutex;
    std::unordered_map<std::string, ReadyNode> ready;
    std::list<std::string> lru_order;  // front=MRU, back=LRU
    std::unordered_map<std::string, std::shared_ptr<InFlight>> in_flight;

    // 将命中节点提升为 MRU（访问刷新 LRU 位置）。
    void touch(decltype(ready)::iterator it) {
        lru_order.splice(lru_order.begin(), lru_order, it->second.lru_pos);
    }

    // 淘汰一个 ready 节点（不涉及 in_flight）。
    void evict_one() {
        if (lru_order.empty()) return;
        const std::string victim = lru_order.back();
        lru_order.pop_back();
        ready.erase(victim);
    }
};

CoalescingCache::CoalescingCache(std::size_t capacity, Clock now)
    : state_(std::make_shared<State>(capacity, std::move(now))) {}

LoadResult CoalescingCache::get_or_load(const std::string& key, std::uint64_t ttl,
                                        const Loader& loader) {
    const auto self = std::this_thread::get_id();
    auto& s = *state_;

    // 1) ready 命中且未过期：刷新 LRU 后直接返回（短锁，loader 不在此执行）。
    {
        std::lock_guard<std::mutex> lk(s.mutex);
        auto it = s.ready.find(key);
        if (it != s.ready.end()) {
            if (s.now() < it->second.expire_time) {
                s.touch(it);
                return LoadResult::ok(it->second.value);
            }
            // 已过期：移除，走加载路径。
            s.lru_order.erase(it->second.lru_pos);
            s.ready.erase(it);
        }
    }

    // 2) 加入已有在途加载，或发起新加载。
    std::shared_ptr<State::InFlight> slot;
    bool am_loader = false;
    {
        std::lock_guard<std::mutex> lk(s.mutex);
        auto fit = s.in_flight.find(key);
        if (fit != s.in_flight.end()) {
            if (fit->second->loader_thread == self) {
                // 递归请求同一 key：不等待自己，立即返回 recursive_load。
                return LoadResult::recursive();
            }
            slot = fit->second;  // 成为等待者，共享这次在途加载
        } else {
            slot = std::make_shared<State::InFlight>();
            slot->loader_thread = self;
            s.in_flight[key] = slot;
            am_loader = true;
        }
    }

    if (!am_loader) {
        // 等待者：在 state 互斥锁之外等待 loader 完成。slot 以 shared_ptr 保活，
        // 即便 loader 已从 in_flight 移除并完成，对象依旧存活。
        std::unique_lock<std::mutex> lk(slot->m);
        slot->cv.wait(lk, [&slot] { return slot->completed; });
        return slot->result;
    }

    // 3) loader 线程：在内部互斥锁之外执行 loader。
    //    重入请求不同 key 会创建/加入各自独立的 slot，互不阻塞。
    LoadResult result;
    try {
        result = loader(key);
    } catch (...) {
        // 异常不跨 API 边界，转为失败结果。
        result = LoadResult::failed("loader threw exception");
    }

    const bool cacheable =
        (result.status == LoadStatus::ok) && (ttl != 0) && (s.capacity != 0);

    // 写回 ready（若可缓存、未被 invalidate 标记），并从在途表移除。
    {
        std::lock_guard<std::mutex> lk(s.mutex);
        const bool discard = slot->discard_result;
        if (cacheable && !discard) {
            // TTL 起点为 loader 完成时的时钟。
            const std::uint64_t complete_clock = s.now();
            const std::uint64_t expire_time = complete_clock + ttl;
            auto it = s.ready.find(key);
            if (it != s.ready.end()) {
                it->second.value = result.value;
                it->second.expire_time = expire_time;
                s.touch(it);
            } else {
                while (s.ready.size() >= s.capacity) {
                    s.evict_one();
                }
                s.lru_order.push_front(key);
                State::ReadyNode node;
                node.value = result.value;
                node.expire_time = expire_time;
                node.lru_pos = s.lru_order.begin();
                s.ready.emplace(key, std::move(node));
            }
        }
        s.in_flight.erase(key);
    }

    // 4) 唤醒所有等待者，发布本次成功或失败结果。
    {
        std::lock_guard<std::mutex> lk(slot->m);
        slot->result = result;  // 拷贝，本函数返回时仍需用
        slot->completed = true;
    }
    slot->cv.notify_all();
    return result;
}

void CoalescingCache::invalidate(const std::string& key) {
    std::lock_guard<std::mutex> lk(state_->mutex);
    // 立即移除 ready 值。
    auto it = state_->ready.find(key);
    if (it != state_->ready.end()) {
        state_->lru_order.erase(it->second.lru_pos);
        state_->ready.erase(it);
    }
    // 若正在加载：不取消 loader，但标记其结果不得写回 ready。
    auto fit = state_->in_flight.find(key);
    if (fit != state_->in_flight.end()) {
        fit->second->discard_result = true;
    }
}

std::size_t CoalescingCache::size() const {
    std::lock_guard<std::mutex> lk(state_->mutex);
    return state_->ready.size();
}
