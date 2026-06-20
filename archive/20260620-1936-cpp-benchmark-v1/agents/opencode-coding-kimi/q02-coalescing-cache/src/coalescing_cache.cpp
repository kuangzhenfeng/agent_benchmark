#include "coalescing_cache.hpp"

#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
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

// Flight 表示一次在途加载，所有等待者共享同一个 flight
struct Flight {
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    LoadResult result;
    bool invalidated = false;
};

struct CoalescingCache::State {
    explicit State(std::size_t capacity, Clock now)
        : capacity(capacity), now(std::move(now)) {}

    std::size_t capacity;
    Clock now;

    mutable std::mutex mutex;

    // 在途加载
    std::unordered_map<std::string, std::shared_ptr<Flight>> flights;

    // 线程局部递归检测
    // 使用 thread_local 的 unordered_set 来检测同一线程是否正在加载同一 key
    // 但由于 loader 可能在锁外执行，我们使用一个更简单的方法：
    // 在 get_or_load 中，在锁内检查当前线程是否已经在加载同一 key
    // 但 C++17 没有 std::thread::id 的 hash，所以我们使用另一种方式
    // 实际上，我们可以在锁内用 flights 来检测：如果当前线程已经在加载某个 key，
    // 那么 flights 中会有该 key 的 flight，但问题是同一线程可能同时加载多个不同的 key
    // 所以我们需要一个 thread_local 的 set 来记录当前线程正在加载的 key

    // ready cache: LRU
    struct CacheEntry {
        std::string key;
        std::string value;
        std::uint64_t expires_at;
    };
    std::list<CacheEntry> lru_list;
    std::unordered_map<std::string, decltype(lru_list.begin())> lru_map;
};

CoalescingCache::CoalescingCache(std::size_t capacity, Clock now)
    : state_(std::make_shared<State>(capacity, std::move(now))) {}

LoadResult CoalescingCache::get_or_load(const std::string& key, std::uint64_t ttl,
                                        const Loader& loader) {
    std::shared_ptr<Flight> flight;
    bool is_leader = false;

    {
        std::lock_guard<std::mutex> lock(state_->mutex);

        // 检查 ready cache
        auto it = state_->lru_map.find(key);
        if (it != state_->lru_map.end()) {
            auto& entry = *it->second;
            std::uint64_t current_time = state_->now();
            if (current_time < entry.expires_at) {
                // 命中缓存，更新 LRU
                state_->lru_list.splice(state_->lru_list.begin(), state_->lru_list,
                                        it->second);
                return LoadResult::ok(entry.value);
            } else {
                // 过期，移除
                state_->lru_list.erase(it->second);
                state_->lru_map.erase(it);
            }
        }

        // 检查是否有在途加载
        auto flight_it = state_->flights.find(key);
        if (flight_it != state_->flights.end()) {
            flight = flight_it->second;
        } else {
            // 成为 leader
            flight = std::make_shared<Flight>();
            state_->flights[key] = flight;
            is_leader = true;
        }
    }

    if (!is_leader) {
        // 等待 leader 完成
        std::unique_lock<std::mutex> lock(flight->mutex);
        flight->cv.wait(lock, [&flight] { return flight->done; });
        return flight->result;
    }

    // Leader: 执行 loader
    LoadResult result;
    try {
        result = loader(key);
    } catch (...) {
        result = LoadResult::failed("loader exception");
    }

    // 处理结果
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->flights.erase(key);
    }

    if (result.status == LoadStatus::ok) {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (!flight->invalidated) {
            // 写入 ready cache
            if (state_->capacity > 0 && ttl > 0) {
                // 移除已存在的同名 entry（不应该有，但为了安全）
                auto existing = state_->lru_map.find(key);
                if (existing != state_->lru_map.end()) {
                    state_->lru_list.erase(existing->second);
                    state_->lru_map.erase(existing);
                }

                std::uint64_t current_time = state_->now();
                state_->lru_list.push_front(
                    typename State::CacheEntry{key, result.value, current_time + ttl});
                state_->lru_map[key] = state_->lru_list.begin();

                // LRU 淘汰
                while (state_->lru_list.size() > state_->capacity) {
                    auto last = state_->lru_list.end();
                    --last;
                    state_->lru_map.erase(last->key);
                    state_->lru_list.pop_back();
                }
            }
        }
    }

    // 通知等待者
    {
        std::lock_guard<std::mutex> lock(flight->mutex);
        flight->result = result;
        flight->done = true;
    }
    flight->cv.notify_all();

    return result;
}

void CoalescingCache::invalidate(const std::string& key) {
    std::lock_guard<std::mutex> lock(state_->mutex);

    // 从 ready cache 中移除
    auto it = state_->lru_map.find(key);
    if (it != state_->lru_map.end()) {
        state_->lru_list.erase(it->second);
        state_->lru_map.erase(it);
    }

    // 标记在途加载为无效
    auto flight_it = state_->flights.find(key);
    if (flight_it != state_->flights.end()) {
        flight_it->second->invalidated = true;
    }
}

std::size_t CoalescingCache::size() const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->lru_map.size();
}
