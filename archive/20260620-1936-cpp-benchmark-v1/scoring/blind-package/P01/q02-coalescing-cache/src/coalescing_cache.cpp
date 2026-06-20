#include "coalescing_cache.hpp"

#include <condition_variable>
#include <cstdlib>
#include <list>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unordered_set>
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

namespace {

struct CacheValue {
    std::string value;
    std::uint64_t loaded_at;
};

struct Flight {
    LoadResult result;
    bool done = false;
    bool should_cache = true;
    std::mutex mtx;
    std::condition_variable cv;
};

}  // namespace

struct CoalescingCache::State {
    std::mutex mtx;
    std::size_t capacity;
    Clock now;

    // ready cache (LRU)
    std::unordered_map<std::string,
                       std::pair<CacheValue, std::list<std::string>::iterator>>
        cache;
    std::list<std::string> lru;

    // in-flight loads
    std::unordered_map<std::string, std::shared_ptr<Flight>> in_flight;

    // 递归加载检测：每个线程当前正在加载的 key
    // thread_local 在 C++11 以上可用
    static thread_local std::unordered_set<std::string> loading_keys;

    State(std::size_t cap, Clock c) : capacity(cap), now(std::move(c)) {}

    void touch(const std::string& key) {
        auto it = cache.find(key);
        if (it != cache.end()) {
            lru.erase(it->second.second);
            lru.push_front(key);
            it->second.second = lru.begin();
        }
    }

    void insert(const std::string& key, const CacheValue& cv) {
        if (capacity == 0)
            return;
        // 已存在则先移除
        auto it = cache.find(key);
        if (it != cache.end()) {
            lru.erase(it->second.second);
            cache.erase(it);
        }
        while (cache.size() >= capacity) {
            auto back = lru.back();
            lru.pop_back();
            cache.erase(back);
        }
        lru.push_front(key);
        cache[key] = {cv, lru.begin()};
    }

    void erase(const std::string& key) {
        auto it = cache.find(key);
        if (it != cache.end()) {
            lru.erase(it->second.second);
            cache.erase(it);
        }
    }
};

thread_local std::unordered_set<std::string>
    CoalescingCache::State::loading_keys;

CoalescingCache::CoalescingCache(std::size_t capacity, Clock now)
    : state_(std::make_shared<State>(capacity, std::move(now))) {}

LoadResult CoalescingCache::get_or_load(const std::string& key,
                                        std::uint64_t ttl,
                                        const Loader& loader) {
    // 递归加载检测
    if (State::loading_keys.find(key) != State::loading_keys.end()) {
        return LoadResult::recursive();
    }

    // Clock 必须在锁外调用
    const std::uint64_t now = state_->now();

    // 1) 检查 ready cache
    {
        std::lock_guard<std::mutex> lock(state_->mtx);
        auto it = state_->cache.find(key);
        if (it != state_->cache.end()) {
            const std::uint64_t age = now - it->second.first.loaded_at;
            if (age < ttl) {
                state_->touch(key);
                return LoadResult::ok(it->second.first.value);
            }
            // 过期，移除
            state_->erase(key);
        }
    }

    // 2) 检查是否已有在途加载
    std::shared_ptr<Flight> flight;
    {
        std::lock_guard<std::mutex> lock(state_->mtx);
        auto fit = state_->in_flight.find(key);
        if (fit != state_->in_flight.end()) {
            flight = fit->second;
        }
    }

    if (flight) {
        // 等待在途加载完成
        std::unique_lock<std::mutex> lock(flight->mtx);
        flight->cv.wait(lock, [&flight] { return flight->done; });
        return flight->result;
    }

    // 3) 创建新的在途加载
    flight = std::make_shared<Flight>();
    {
        std::lock_guard<std::mutex> lock(state_->mtx);
        state_->in_flight[key] = flight;
    }

    // 执行 loader（锁外）
    State::loading_keys.insert(key);
    LoadResult result;
    try {
        result = loader(key);
    } catch (...) {
        result = LoadResult::failed("loader threw");
    }
    State::loading_keys.erase(key);

    // 获取加载完成时的时钟（锁外）
    const std::uint64_t loaded_at = state_->now();

    // 最终化：确定结果、写入 cache（若适用）、通知等待者、从 in_flight 移除
    {
        std::lock_guard<std::mutex> lock(state_->mtx);

        // 确定是否应写入 ready cache
        const bool should_store =
            flight->should_cache && result.status == LoadStatus::ok && ttl > 0;

        // 设置 flight 结果（在 flight mutex 下）
        {
            std::lock_guard<std::mutex> flight_lock(flight->mtx);
            flight->result = result;
            flight->done = true;
        }
        flight->cv.notify_all();

        // 写入 ready cache（若适用）
        if (should_store) {
            state_->insert(key, CacheValue{result.value, loaded_at});
        }

        // 从 in_flight 移除
        state_->in_flight.erase(key);
    }

    return result;
}

void CoalescingCache::invalidate(const std::string& key) {
    std::lock_guard<std::mutex> lock(state_->mtx);
    state_->erase(key);
    auto fit = state_->in_flight.find(key);
    if (fit != state_->in_flight.end()) {
        fit->second->should_cache = false;
    }
}

std::size_t CoalescingCache::size() const {
    std::lock_guard<std::mutex> lock(state_->mtx);
    return state_->cache.size();
}