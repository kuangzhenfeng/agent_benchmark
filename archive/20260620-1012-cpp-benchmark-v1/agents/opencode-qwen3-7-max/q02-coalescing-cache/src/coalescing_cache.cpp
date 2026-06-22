#include "coalescing_cache.hpp"

#include <condition_variable>
#include <list>
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

struct CoalescingCache::State {
    struct FlightFuture {
        std::mutex done_mtx;
        std::condition_variable done_cv;
        bool result_set = false;
        LoadResult result;
        bool invalidated = false;
    };

    struct ReadyEntry {
        std::string value;
        std::uint64_t complete_time;
    };

    State(std::size_t cap, Clock clk)
        : capacity(cap), clock(std::move(clk)) {}

    ~State() {
        flight_map.clear();
        ready_order.clear();
        ready_cache.clear();
    }

    std::size_t capacity;
    Clock clock;

    std::mutex mtx;
    std::unordered_map<std::string, std::shared_ptr<FlightFuture>> flight_map;
    std::list<std::string> ready_order;
    std::unordered_map<std::string, ReadyEntry> ready_cache;
};

static thread_local std::unordered_set<std::string> tl_loading_keys;

CoalescingCache::CoalescingCache(std::size_t capacity, Clock now)
    : state_(std::make_shared<State>(capacity, std::move(now))) {}

LoadResult CoalescingCache::get_or_load(const std::string& key,
                                       std::uint64_t ttl,
                                       const Loader& loader) {
    if (tl_loading_keys.count(key)) {
        return LoadResult::recursive();
    }

    std::string cached_value;
    std::uint64_t cached_complete_time = 0;
    {
        std::lock_guard<std::mutex> lock(state_->mtx);
        auto it = state_->ready_cache.find(key);
        if (it != state_->ready_cache.end()) {
            cached_value = it->second.value;
            cached_complete_time = it->second.complete_time;
        }
    }

    if (!cached_value.empty() || cached_complete_time != 0) {
        std::uint64_t now_val = state_->clock();
        std::uint64_t elapsed = now_val - cached_complete_time;
        if (elapsed < ttl) {
            std::lock_guard<std::mutex> lock(state_->mtx);
            auto it = state_->ready_cache.find(key);
            if (it != state_->ready_cache.end()) {
                state_->ready_order.remove(key);
                state_->ready_order.push_back(key);
            }
            return LoadResult::ok(cached_value);
        }
    }

    std::shared_ptr<State::FlightFuture> future;
    bool is_owner = false;
    {
        std::lock_guard<std::mutex> lock(state_->mtx);
        auto it = state_->flight_map.find(key);
        if (it != state_->flight_map.end()) {
            future = it->second;
        } else {
            future = std::make_shared<State::FlightFuture>();
            state_->flight_map.emplace(key, future);
            is_owner = true;
        }
    }

    if (!is_owner) {
        std::unique_lock<std::mutex> lk(future->done_mtx);
        future->done_cv.wait(lk, [&] { return future->result_set; });
        return future->result;
    }

    tl_loading_keys.insert(key);
    LoadResult result;
    try {
        result = loader(key);
    } catch (const std::exception& e) {
        result = LoadResult::failed(e.what());
    } catch (...) {
        result = LoadResult::failed("unknown exception");
    }
    tl_loading_keys.erase(key);

    std::uint64_t complete_time = 0;
    try { complete_time = state_->clock(); } catch (...) {}

    bool should_cache = (result.status == LoadStatus::ok) && (ttl > 0);
    bool flight_invalidated = false;

    {
        std::lock_guard<std::mutex> lock(state_->mtx);
        if (should_cache) {
            state_->ready_cache.erase(key);
            state_->ready_order.remove(key);
        }
        auto flight_it = state_->flight_map.find(key);
        if (flight_it != state_->flight_map.end()) {
            flight_invalidated = flight_it->second->invalidated;
        }
        if (should_cache && !flight_invalidated) {
            state_->ready_order.push_back(key);
            state_->ready_cache.emplace(
                key, State::ReadyEntry{result.value, complete_time});
            while (state_->ready_cache.size() > state_->capacity) {
                const std::string& lru_key = state_->ready_order.front();
                state_->ready_cache.erase(lru_key);
                state_->ready_order.pop_front();
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(future->done_mtx);
        future->result = result;
        future->result_set = true;
    }
    future->done_cv.notify_all();

    {
        std::lock_guard<std::mutex> lock(state_->mtx);
        state_->flight_map.erase(key);
    }

    return result;
}

void CoalescingCache::invalidate(const std::string& key) {
    std::lock_guard<std::mutex> lock(state_->mtx);
    state_->ready_cache.erase(key);
    state_->ready_order.remove(key);
    auto it = state_->flight_map.find(key);
    if (it != state_->flight_map.end()) {
        std::lock_guard<std::mutex> flight_lock(it->second->done_mtx);
        it->second->invalidated = true;
    }
}

std::size_t CoalescingCache::size() const {
    std::lock_guard<std::mutex> lock(state_->mtx);
    return state_->ready_cache.size();
}
