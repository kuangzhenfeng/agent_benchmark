#include "coalescing_cache.hpp"

#include <condition_variable>
#include <list>
#include <mutex>
#include <stdexcept>
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

struct Flight {
    std::mutex mu;
    std::condition_variable cv;
    bool done = false;
    LoadResult result;
    bool invalidated = false;
};

struct ReadyEntry {
    std::string value;
    std::uint64_t loaded_at;
    std::uint64_t ttl;
};

}  // namespace

struct CoalescingCache::State {
    explicit State(std::size_t cap, Clock fn)
        : capacity(cap), now(std::move(fn)) {}

    std::mutex mu;
    std::size_t capacity;
    Clock now;

    std::unordered_map<std::string, ReadyEntry> ready;
    std::list<std::string> lru_list;
    std::unordered_map<std::string, std::list<std::string>::iterator> lru_map;

    std::unordered_map<std::string, std::shared_ptr<Flight>> inflight;

    void evict_lru() {
        while (capacity > 0 && ready.size() >= capacity) {
            auto victim = lru_list.front();
            lru_list.pop_front();
            lru_map.erase(victim);
            ready.erase(victim);
        }
    }

    void insert_ready(const std::string& key, std::string value,
                      std::uint64_t loaded_at, std::uint64_t ttl) {
        if (capacity == 0) return;
        auto it = ready.find(key);
        if (it != ready.end()) {
            it->second = ReadyEntry{std::move(value), loaded_at, ttl};
            lru_list.erase(lru_map[key]);
            lru_list.push_back(key);
            lru_map[key] = std::prev(lru_list.end());
            return;
        }
        evict_lru();
        if (capacity == 0) return;
        ready.emplace(key, ReadyEntry{std::move(value), loaded_at, ttl});
        lru_list.push_back(key);
        lru_map[key] = std::prev(lru_list.end());
    }

    void erase_ready(const std::string& key) {
        auto it = ready.find(key);
        if (it != ready.end()) {
            lru_list.erase(lru_map[key]);
            lru_map.erase(key);
            ready.erase(it);
        }
    }

    void touch_ready(const std::string& key) {
        lru_list.erase(lru_map[key]);
        lru_list.push_back(key);
        lru_map[key] = std::prev(lru_list.end());
    }
};

CoalescingCache::CoalescingCache(std::size_t capacity, Clock now)
    : state_(std::make_shared<State>(capacity, std::move(now))) {}

LoadResult CoalescingCache::get_or_load(const std::string& key, std::uint64_t ttl,
                                        const Loader& loader) {
    thread_local std::unordered_set<std::string> active_loads;

    if (active_loads.count(key)) {
        return LoadResult::recursive();
    }

    auto now_val = state_->now();

    {
        std::lock_guard<std::mutex> lock(state_->mu);
        auto it = state_->ready.find(key);
        if (it != state_->ready.end()) {
            const std::uint64_t elapsed = now_val - it->second.loaded_at;
            if (elapsed < it->second.ttl) {
                state_->touch_ready(key);
                return LoadResult::ok(it->second.value);
            }
            state_->erase_ready(key);
        }
    }

    std::shared_ptr<Flight> flight;
    bool is_owner = false;
    {
        std::lock_guard<std::mutex> lock(state_->mu);
        auto it = state_->inflight.find(key);
        if (it != state_->inflight.end()) {
            flight = it->second;
        } else {
            flight = std::make_shared<Flight>();
            state_->inflight.emplace(key, flight);
            is_owner = true;
        }
    }

    if (!is_owner) {
        std::unique_lock<std::mutex> flk(flight->mu);
        flight->cv.wait(flk, [&] { return flight->done; });
        return flight->result;
    }

    active_loads.insert(key);

    LoadResult loader_result;
    try {
        loader_result = loader(key);
    } catch (const std::exception& e) {
        loader_result = LoadResult::failed(e.what());
    } catch (...) {
        loader_result = LoadResult::failed("unknown exception");
    }

    auto completed_at = state_->now();

    {
        std::lock_guard<std::mutex> lock(state_->mu);
        std::lock_guard<std::mutex> flk(flight->mu);

        flight->result = loader_result;

        if (!flight->invalidated && loader_result.status == LoadStatus::ok && ttl > 0) {
            state_->insert_ready(key, loader_result.value, completed_at, ttl);
        }

        state_->inflight.erase(key);
        flight->done = true;
    }
    flight->cv.notify_all();

    active_loads.erase(key);

    return loader_result;
}

void CoalescingCache::invalidate(const std::string& key) {
    std::shared_ptr<Flight> flight_to_mark;
    {
        std::lock_guard<std::mutex> lock(state_->mu);
        state_->erase_ready(key);
        auto it = state_->inflight.find(key);
        if (it != state_->inflight.end()) {
            flight_to_mark = it->second;
        }
    }
    if (flight_to_mark) {
        std::lock_guard<std::mutex> flk(flight_to_mark->mu);
        flight_to_mark->invalidated = true;
    }
}

std::size_t CoalescingCache::size() const {
    std::lock_guard<std::mutex> lock(state_->mu);
    return state_->ready.size();
}
