#include "coalescing_cache.hpp"

#include <condition_variable>
#include <list>
#include <unordered_map>
#include <utility>
#include <vector>

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

struct Entry {
    std::string value;
    std::uint64_t expire_tick = 0;
};

// A flight represents a single in-flight load. Its `result`, `done` and
// `invalidated` fields are accessed ONLY under CoalescingCache::State::mutex
// (the same mutex that governs discovery in the flights_ map), so every caller
// that discovers the flight observes a complete, identical result via one
// synchronization relationship (requirement 7). No separate "done/result"
// mutex exists.
struct Flight {
    LoadResult result{LoadStatus::loader_failed, {}, {}};
    bool done = false;
    bool invalidated = false;
};

// Per-thread stack of flights currently being loaded by THIS thread. Used to
// detect a loader that re-enters get_or_load for the same key (recursive load).
// Raw pointers are non-owning and only valid between push and pop on the same
// thread, so no ABA is possible.
thread_local std::vector<Flight*> active_loads;

bool is_active_load(const Flight* f) {
    for (const Flight* a : active_loads) {
        if (a == f) return true;
    }
    return false;
}

}  // namespace

struct CoalescingCache::State {
    explicit State(std::size_t capacity, Clock now)
        : capacity(capacity), now(std::move(now)) {}

    using LruIter = std::list<std::pair<std::string, Entry>>::iterator;

    std::size_t capacity;
    Clock now;
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::list<std::pair<std::string, Entry>> lru;          // front == MRU
    std::unordered_map<std::string, LruIter> ready_index;  // ready cache only
    std::unordered_map<std::string, std::shared_ptr<Flight>> flights;
};

CoalescingCache::CoalescingCache(std::size_t capacity, Clock now)
    : state_(std::make_shared<State>(capacity, std::move(now))) {}

LoadResult CoalescingCache::get_or_load(const std::string& key, std::uint64_t ttl,
                                        const Loader& loader) {
    // Clock is user code: call it OUTSIDE the mutex. It may re-enter
    // invalidate() or request a different key; both are safe because we hold
    // no lock here.
    const std::uint64_t now_entry = state_->now();

    std::unique_lock<std::mutex> lock(state_->mutex);

    // 1) Ready cache (only meaningful when ttl > 0).
    if (ttl > 0) {
        const auto it = state_->ready_index.find(key);
        if (it != state_->ready_index.end()) {
            Entry& entry = it->second->second;
            if (now_entry < entry.expire_tick) {
                // Hit: refresh LRU position, return a self-contained copy.
                state_->lru.splice(state_->lru.begin(), state_->lru, it->second);
                it->second = state_->lru.begin();
                return LoadResult::ok(entry.value);
            }
            // Expired: drop and fall through to load.
            state_->lru.erase(it->second);
            state_->ready_index.erase(it);
        }
    }

    // 2) Join an in-flight load, if any.
    const auto fit = state_->flights.find(key);
    if (fit != state_->flights.end()) {
        std::shared_ptr<Flight> flight = fit->second;  // keep alive
        if (is_active_load(flight.get())) {
            // This thread is already the owner of this flight (recursive
            // same-key load). Return immediately without waiting on itself.
            return LoadResult::recursive();
        }
        // Wait until the owner publishes (owner sets done + erases from map
        // + notifies, all under this same mutex).
        state_->cv.wait(lock, [&flight] { return flight->done; });
        return flight->result;  // copy under lock; consistent with owner
    }

    // 3) Become the owner of a new flight.
    auto flight = std::make_shared<Flight>();
    state_->flights[key] = flight;
    active_loads.push_back(flight.get());

    // Loader runs entirely outside the mutex (user code, may re-enter).
    lock.unlock();
    LoadResult result{LoadStatus::loader_failed, {}, "loader threw"};
    std::uint64_t now_done = now_entry;
    try {
        result = loader(key);
        // Completion clock also outside the mutex; this is the TTL start.
        now_done = state_->now();
    } catch (...) {
        result = LoadResult::failed("loader threw");
        // On exception we do not cache, so a completion timestamp is unused.
    }

    // Publish: a single atomic transition under the mutex. Before the flight
    // becomes undiscoverable (erase), we have finalized the result, decided
    // the ready-cache write (honoring any invalidate()), and will notify.
    lock.lock();
    flight->result = result;
    flight->done = true;
    const bool cacheable = result.status == LoadStatus::ok && ttl > 0 &&
                           !flight->invalidated && state_->capacity > 0;
    if (cacheable) {
        const auto it = state_->ready_index.find(key);
        if (it != state_->ready_index.end()) {
            Entry& entry = it->second->second;
            entry.value = result.value;
            entry.expire_tick = now_done + ttl;
            state_->lru.splice(state_->lru.begin(), state_->lru, it->second);
            it->second = state_->lru.begin();
        } else {
            state_->lru.push_front({key, Entry{result.value, now_done + ttl}});
            state_->ready_index[key] = state_->lru.begin();
            while (state_->ready_index.size() > state_->capacity) {
                const auto& back = state_->lru.back();
                state_->ready_index.erase(back.first);
                state_->lru.pop_back();
            }
        }
    }
    state_->flights.erase(key);  // flight now undiscoverable
    active_loads.pop_back();
    state_->cv.notify_all();     // wake every waiter on this cache
    return result;
}

void CoalescingCache::invalidate(const std::string& key) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    const auto it = state_->ready_index.find(key);
    if (it != state_->ready_index.end()) {
        state_->lru.erase(it->second);
        state_->ready_index.erase(it);
    }
    // If a load is in flight, mark it so the owner skips the ready-cache
    // writeback. We do NOT cancel the loader; existing waiters still receive
    // the result, and new callers may still join this flight.
    const auto fit = state_->flights.find(key);
    if (fit != state_->flights.end()) {
        fit->second->invalidated = true;
    }
}

std::size_t CoalescingCache::size() const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->ready_index.size();
}
