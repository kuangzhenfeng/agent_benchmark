#include "coalescing_cache.hpp"

#include <condition_variable>
#include <list>
#include <mutex>
#include <stdexcept>
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
    struct ReadyEntry {
        std::string value;
        std::uint64_t expires_at;
    };

    struct InFlight {
        std::mutex inflight_mtx;
        std::condition_variable cv;
        bool done = false;
        LoadResult result{LoadStatus::loader_failed, {}, "pending"};
        bool invalidated = false;
        std::thread::id loader_thread;
    };

    std::size_t capacity;
    Clock now;

    mutable std::mutex mtx;

    // Ready cache: LRU list (front = MRU, back = LRU) + index
    std::list<std::pair<std::string, ReadyEntry>> lru_list;
    std::unordered_map<std::string,
                       decltype(lru_list)::iterator> ready_map;

    // In-flight loads
    std::unordered_map<std::string,
                       std::shared_ptr<InFlight>> inflight_map;

    State(std::size_t cap, Clock clock)
        : capacity(cap), now(std::move(clock)) {}

    // --- helpers, caller holds mtx ---

    void evict_lru() {
        if (lru_list.empty()) return;
        ready_map.erase(lru_list.back().first);
        lru_list.pop_back();
    }

    void add_to_ready(const std::string& key, std::string value,
                      std::uint64_t expires_at) {
        if (capacity == 0) return;

        auto it = ready_map.find(key);
        if (it != ready_map.end()) {
            it->second->second.value = std::move(value);
            it->second->second.expires_at = expires_at;
            lru_list.splice(lru_list.begin(), lru_list, it->second);
            return;
        }

        while (ready_map.size() >= capacity && !lru_list.empty()) {
            evict_lru();
        }

        lru_list.push_front({key, ReadyEntry{std::move(value), expires_at}});
        ready_map[key] = lru_list.begin();
    }

    void remove_from_ready(const std::string& key) {
        auto it = ready_map.find(key);
        if (it != ready_map.end()) {
            lru_list.erase(it->second);
            ready_map.erase(it);
        }
    }
};

// ---------- public API ----------

CoalescingCache::CoalescingCache(std::size_t capacity, Clock now)
    : state_(std::make_shared<State>(capacity, std::move(now))) {}

LoadResult CoalescingCache::get_or_load(const std::string& key,
                                        std::uint64_t ttl,
                                        const Loader& loader) {
    std::shared_ptr<State::InFlight> inflight;
    bool is_owner = false;

    {
        std::lock_guard<std::mutex> lock(state_->mtx);

        // 1. Check ready cache
        auto rit = state_->ready_map.find(key);
        if (rit != state_->ready_map.end()) {
            auto& entry = rit->second->second;
            if (state_->now() < entry.expires_at) {
                // Hit – refresh LRU position
                state_->lru_list.splice(state_->lru_list.begin(),
                                        state_->lru_list, rit->second);
                return LoadResult::ok(entry.value);
            }
            // Expired – remove
            state_->lru_list.erase(rit->second);
            state_->ready_map.erase(rit);
        }

        // 2. Check in-flight map
        auto fit = state_->inflight_map.find(key);
        if (fit != state_->inflight_map.end()) {
            inflight = fit->second;

            // Recursive-load detection: same thread, still loading
            if (!inflight->done &&
                inflight->loader_thread == std::this_thread::get_id()) {
                return LoadResult::recursive();
            }

            if (inflight->done) {
                // Owner already finished (rare race window) – grab result
                return inflight->result;
            }

            // Another thread's load – fall through to wait
        } else {
            // No in-flight load – we become the owner
            inflight = std::make_shared<State::InFlight>();
            inflight->loader_thread = std::this_thread::get_id();
            state_->inflight_map.emplace(key, inflight);
            is_owner = true;
        }
    }

    if (!is_owner) {
        // Wait for the owner to finish
        std::unique_lock<std::mutex> lock(inflight->inflight_mtx);
        inflight->cv.wait(lock, [&] { return inflight->done; });
        return inflight->result;
    }

    // ---- Owner path: execute loader outside any mutex ----

    LoadResult result;
    try {
        result = loader(key);
    } catch (const std::exception& e) {
        result = LoadResult::failed(e.what());
    } catch (...) {
        result = LoadResult::failed("unknown exception");
    }

    // TTL starts at loader completion
    const auto completion_time = state_->now();

    // Publish result to waiters
    {
        std::lock_guard<std::mutex> lock(inflight->inflight_mtx);
        inflight->result = result;
        inflight->done = true;
    }
    inflight->cv.notify_all();

    // Write to ready cache (if applicable) and clean up in-flight entry
    {
        std::lock_guard<std::mutex> lock(state_->mtx);

        if (!inflight->invalidated &&
            result.status == LoadStatus::ok && ttl > 0) {
            state_->add_to_ready(key, result.value, completion_time + ttl);
        }

        state_->inflight_map.erase(key);
    }

    return result;
}

void CoalescingCache::invalidate(const std::string& key) {
    std::lock_guard<std::mutex> lock(state_->mtx);

    state_->remove_from_ready(key);

    auto fit = state_->inflight_map.find(key);
    if (fit != state_->inflight_map.end()) {
        // Lock ordering: state_->mtx → inflight->inflight_mtx (consistent)
        std::lock_guard<std::mutex> ilock(fit->second->inflight_mtx);
        fit->second->invalidated = true;
    }
}

std::size_t CoalescingCache::size() const {
    std::lock_guard<std::mutex> lock(state_->mtx);
    return state_->ready_map.size();
}
