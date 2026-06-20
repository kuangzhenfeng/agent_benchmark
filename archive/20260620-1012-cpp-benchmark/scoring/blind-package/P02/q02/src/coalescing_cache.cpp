#include "coalescing_cache.hpp"

#include <condition_variable>
#include <exception>
#include <list>
#include <mutex>
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

struct CoalescingCache::State {
    explicit State(std::size_t capacity, Clock now)
        : capacity(capacity), now(std::move(now)) {}

    struct ReadyEntry {
        std::string key;
        std::string value;
        std::uint64_t completed_at;
        std::uint64_t ttl;
    };

    struct Flight {
        bool done = false;
        bool invalidated = false;
        LoadResult result;
        std::condition_variable completed;
    };

    std::size_t capacity;
    Clock now;
    mutable std::mutex mutex;
    std::list<ReadyEntry> lru;
    std::unordered_map<std::string, std::list<ReadyEntry>::iterator> ready;
    std::unordered_map<std::string, std::shared_ptr<Flight>> flights;
};

namespace {

struct ActiveCall {
    const void* state;
    std::string key;
};

thread_local std::vector<ActiveCall> active_calls;

bool is_active(const void* state, const std::string& key) {
    for (const ActiveCall& call : active_calls) {
        if (call.state == state && call.key == key) {
            return true;
        }
    }
    return false;
}

class ActiveCallGuard {
public:
    ActiveCallGuard(const void* state, const std::string& key) {
        active_calls.push_back(ActiveCall{state, key});
    }

    ~ActiveCallGuard() {
        active_calls.pop_back();
    }

    ActiveCallGuard(const ActiveCallGuard&) = delete;
    ActiveCallGuard& operator=(const ActiveCallGuard&) = delete;
};

LoadResult clock_failure() {
    return LoadResult::failed("clock threw");
}

}  // namespace

CoalescingCache::CoalescingCache(std::size_t capacity, Clock now)
    : state_(std::make_shared<State>(capacity, std::move(now))) {}

LoadResult CoalescingCache::get_or_load(const std::string& key, std::uint64_t ttl,
                                        const Loader& loader) {
    // A state copy keeps storage alive if a loader happens to hold the last
    // useful reference to its cache.  Recursive entry is rejected before any
    // wait so an owner never waits for itself.
    const std::shared_ptr<State> state = state_;
    if (is_active(state.get(), key)) {
        return LoadResult::recursive();
    }
    ActiveCallGuard call_guard(state.get(), key);

    auto wait_for_flight = [](std::unique_lock<std::mutex>& lock,
                              const std::shared_ptr<State::Flight>& active_flight) {
        active_flight->completed.wait(lock, [&active_flight] {
            return active_flight->done;
        });
        // Flight::result and Flight::done are read and written only under the
        // State mutex, which is also the condition-variable mutex.
        return active_flight->result;
    };
    auto erase_ready = [&state](auto found) {
        state->lru.erase(found->second);
        state->ready.erase(found);
    };
    auto store_ready = [&state, &erase_ready](const std::string& ready_key,
                                              const LoadResult& result,
                                              std::uint64_t completed_at,
                                              std::uint64_t ready_ttl) {
        const auto existing = state->ready.find(ready_key);
        if (existing != state->ready.end()) {
            erase_ready(existing);
        }
        state->lru.push_front(State::ReadyEntry{
            ready_key, result.value, completed_at, ready_ttl});
        state->ready.emplace(ready_key, state->lru.begin());
        while (state->ready.size() > state->capacity) {
            const auto oldest = std::prev(state->lru.end());
            state->ready.erase(oldest->key);
            state->lru.erase(oldest);
        }
    };

    std::shared_ptr<State::Flight> flight;
    {
        std::unique_lock<std::mutex> lock(state->mutex);
        const auto found = state->flights.find(key);
        if (found != state->flights.end()) {
            return wait_for_flight(lock, found->second);
        }
    }

    // Clock is user code.  It is intentionally called after releasing the
    // cache mutex and before the ready-entry freshness check.
    std::uint64_t now = 0;
    try {
        now = state->now();
    } catch (...) {
        std::unique_lock<std::mutex> lock(state->mutex);
        const auto found = state->flights.find(key);
        if (found != state->flights.end()) {
            return wait_for_flight(lock, found->second);
        }
        return clock_failure();
    }

    {
        std::unique_lock<std::mutex> lock(state->mutex);

        // A flight can have been created while Clock was executing.  Joining
        // it preserves single-flight behaviour even under clock re-entry.
        const auto in_flight = state->flights.find(key);
        if (in_flight != state->flights.end()) {
            return wait_for_flight(lock, in_flight->second);
        }

        const auto cached = state->ready.find(key);
        if (cached != state->ready.end()) {
            if (now - cached->second->completed_at < cached->second->ttl) {
                state->lru.splice(state->lru.begin(), state->lru, cached->second);
                return LoadResult::ok(cached->second->value);
            }
            erase_ready(cached);
        }

        flight = std::make_shared<State::Flight>();
        state->flights.emplace(key, flight);
    }

    // Only the thread that inserted the flight reaches here.  Loader calls
    // and all final callback destruction stay outside cache locks.
    LoadResult final_result;
    try {
        final_result = loader(key);
    } catch (const std::exception& error) {
        final_result = LoadResult::failed(error.what());
    } catch (...) {
        final_result = LoadResult::failed("loader threw");
    }

    std::uint64_t completed_at = 0;
    bool have_completion_time = false;
    if (final_result.status == LoadStatus::ok) {
        try {
            completed_at = state->now();
            have_completion_time = true;
        } catch (...) {
            final_result = clock_failure();
        }
    }

    {
        std::lock_guard<std::mutex> lock(state->mutex);
        // The final result, cache-write decision, and removal from flights are
        // one mutex-protected transition.  Therefore a newly arriving caller
        // sees either this published ready value or a new flight, never a
        // partially-completed old one.
        const bool cacheable = final_result.status == LoadStatus::ok &&
                               have_completion_time && ttl != 0 &&
                               state->capacity != 0 && !flight->invalidated;
        if (cacheable) {
            store_ready(key, final_result, completed_at, ttl);
        }
        flight->result = final_result;
        flight->done = true;

        const auto found = state->flights.find(key);
        if (found != state->flights.end() && found->second == flight) {
            state->flights.erase(found);
        }
    }
    flight->completed.notify_all();
    return final_result;
}

void CoalescingCache::invalidate(const std::string& key) {
    const std::shared_ptr<State> state = state_;
    std::lock_guard<std::mutex> lock(state->mutex);
    const auto cached = state->ready.find(key);
    if (cached != state->ready.end()) {
        state->lru.erase(cached->second);
        state->ready.erase(cached);
    }
    const auto in_flight = state->flights.find(key);
    if (in_flight != state->flights.end()) {
        // Existing waiters still consume the result, but this generation must
        // not restore a value invalidated while it was loading.
        in_flight->second->invalidated = true;
    }
}

std::size_t CoalescingCache::size() const {
    const std::shared_ptr<State> state = state_;
    std::lock_guard<std::mutex> lock(state->mutex);
    return state->ready.size();
}
