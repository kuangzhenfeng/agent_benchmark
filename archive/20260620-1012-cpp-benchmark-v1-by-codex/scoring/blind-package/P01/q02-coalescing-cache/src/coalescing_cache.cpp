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

// ---------------------------------------------------------------------------
// State: all mutable cache state lives here.
// ---------------------------------------------------------------------------

struct CoalescingCache::State {
    explicit State(std::size_t capacity, Clock now)
        : capacity(capacity), now(std::move(now)) {}

    struct ReadyEntry {
        std::string key;
        std::string value;
        std::uint64_t expires_at;
    };

    struct Flight {
        std::mutex mtx;              // protects done, result
        std::condition_variable cv;
        bool done = false;
        LoadResult result;
        std::thread::id owner_tid;   // set once at creation, read-only after
        bool write_back = true;      // protected by State::mtx
    };

    std::size_t capacity;
    Clock now;

    mutable std::mutex mtx;
    std::list<ReadyEntry> lru_list;   // front = most recently used
    std::unordered_map<std::string, std::list<ReadyEntry>::iterator> ready_map;
    std::unordered_map<std::string, std::shared_ptr<Flight>> in_flight;
};

// ---------------------------------------------------------------------------

CoalescingCache::CoalescingCache(std::size_t capacity, Clock now)
    : state_(std::make_shared<State>(capacity, std::move(now))) {}

// ---------------------------------------------------------------------------

LoadResult CoalescingCache::get_or_load(const std::string& key,
                                        std::uint64_t ttl,
                                        const Loader& loader) {
    // Clock called OUTSIDE any lock (requirement 2).
    const std::uint64_t start_time = state_->now();

    std::shared_ptr<State::Flight> flight;
    bool is_owner = false;

    {
        std::lock_guard<std::mutex> lock(state_->mtx);

        // --- Ready cache lookup ---
        auto it = state_->ready_map.find(key);
        if (it != state_->ready_map.end()) {
            auto& entry = *it->second;
            if (start_time < entry.expires_at) {
                state_->lru_list.splice(state_->lru_list.begin(),
                                        state_->lru_list, it->second);
                return LoadResult::ok(entry.value);
            }
            // Expired.
            state_->lru_list.erase(it->second);
            state_->ready_map.erase(it);
        }

        // --- In-flight lookup ---
        auto fit = state_->in_flight.find(key);
        if (fit != state_->in_flight.end()) {
            flight = fit->second;
            if (flight->owner_tid == std::this_thread::get_id()) {
                return LoadResult::recursive();
            }
            is_owner = false;
        } else {
            flight = std::make_shared<State::Flight>();
            flight->owner_tid = std::this_thread::get_id();
            state_->in_flight.emplace(key, flight);
            is_owner = true;
        }
    }

    // --- Waiters block on the flight ---
    if (!is_owner) {
        std::unique_lock<std::mutex> flock(flight->mtx);
        flight->cv.wait(flock, [&] { return flight->done; });
        return flight->result;
    }

    // --- Owner runs loader OUTSIDE any lock (requirement 2) ---
    LoadResult result;
    try {
        result = loader(key);
    } catch (const std::exception& e) {
        result = LoadResult::failed(e.what());
    } catch (...) {
        result = LoadResult::failed("unknown exception");
    }

    // Clock for completion time, OUTSIDE lock.
    const std::uint64_t completion_time = state_->now();

    // --- Finalize (requirement 7) ---
    // The flight remains discoverable in in_flight until the ready cache is
    // written and the flight is removed atomically under state_->mtx.
    //
    // Sequence:
    //   1. Set result on flight (under flight->mtx) — waiters can now observe.
    //   2. Write ready cache + remove from in_flight (under state_->mtx).
    //   3. Notify waiters.

    // Step 1: publish result to flight object.
    {
        std::lock_guard<std::mutex> flock(flight->mtx);
        flight->result = result;
        flight->done = true;
    }

    // Step 2: conditionally write ready cache, then remove flight.
    {
        std::lock_guard<std::mutex> lock(state_->mtx);

        const bool should_cache =
            (result.status == LoadStatus::ok &&
             flight->write_back &&
             state_->capacity > 0 &&
             ttl > 0);

        if (should_cache) {
            const std::uint64_t expires_at = completion_time + ttl;

            // Remove stale entry for same key if present.
            auto old = state_->ready_map.find(key);
            if (old != state_->ready_map.end()) {
                state_->lru_list.erase(old->second);
                state_->ready_map.erase(old);
            }

            // LRU eviction.
            while (state_->lru_list.size() >= state_->capacity) {
                state_->ready_map.erase(state_->lru_list.back().key);
                state_->lru_list.pop_back();
            }

            state_->lru_list.push_front(
                State::ReadyEntry{key, result.value, expires_at});
            state_->ready_map[key] = state_->lru_list.begin();
        }

        // Remove flight — new callers either see ready value or start fresh.
        state_->in_flight.erase(key);
    }

    // Step 3: wake all waiters.
    flight->cv.notify_all();

    return result;
}

// ---------------------------------------------------------------------------

void CoalescingCache::invalidate(const std::string& key) {
    std::lock_guard<std::mutex> lock(state_->mtx);

    auto it = state_->ready_map.find(key);
    if (it != state_->ready_map.end()) {
        state_->lru_list.erase(it->second);
        state_->ready_map.erase(it);
    }

    // Suppress write-back for any in-flight loader on this key.
    auto fit = state_->in_flight.find(key);
    if (fit != state_->in_flight.end()) {
        fit->second->write_back = false;
    }
}

// ---------------------------------------------------------------------------

std::size_t CoalescingCache::size() const {
    std::lock_guard<std::mutex> lock(state_->mtx);
    return state_->ready_map.size();
}
