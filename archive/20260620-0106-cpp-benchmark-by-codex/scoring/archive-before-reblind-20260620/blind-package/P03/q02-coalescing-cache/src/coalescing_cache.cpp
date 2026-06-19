#include "coalescing_cache.hpp"

#include <condition_variable>
#include <exception>
#include <list>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct ActiveLoad {
    const void* state;
    std::string key;
};

thread_local std::vector<ActiveLoad> active_loads;

bool is_loading_on_this_thread(const void* state, const std::string& key) {
    for (const auto& load : active_loads) {
        if (load.state == state && load.key == key) {
            return true;
        }
    }
    return false;
}

class ActiveLoadScope {
public:
    ActiveLoadScope(const void* state, const std::string& key) {
        active_loads.push_back(ActiveLoad{state, key});
    }

    ~ActiveLoadScope() {
        active_loads.pop_back();
    }

    ActiveLoadScope(const ActiveLoadScope&) = delete;
    ActiveLoadScope& operator=(const ActiveLoadScope&) = delete;
};

}  // namespace

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

    struct ReadyValue {
        std::string value;
        std::uint64_t loaded_at;
        std::uint64_t ttl;
        std::list<std::string>::iterator lru_position;
    };

    struct InFlight {
        bool done = false;
        bool cacheable = true;
        LoadResult result = LoadResult::failed("loader did not complete");
        std::condition_variable completed;
    };

    std::size_t capacity;
    Clock now;
    mutable std::mutex mutex;
    std::list<std::string> lru;
    std::unordered_map<std::string, ReadyValue> ready;
    std::unordered_map<std::string, std::shared_ptr<InFlight>> in_flight;
};

CoalescingCache::CoalescingCache(std::size_t capacity, Clock now)
    : state_(std::make_shared<State>(capacity, std::move(now))) {}

LoadResult CoalescingCache::get_or_load(const std::string& key, std::uint64_t ttl,
                                        const Loader& loader) {
    const auto state = state_;
    if (is_loading_on_this_thread(state.get(), key)) {
        return LoadResult::recursive();
    }

    // Calling the clock can execute user code, so never do it while the cache
    // mutex is locked.
    std::uint64_t observed_now = 0;
    try {
        observed_now = state->now();
    } catch (const std::exception& error) {
        return LoadResult::failed(error.what());
    } catch (...) {
        return LoadResult::failed("clock threw an unknown exception");
    }

    std::shared_ptr<State::InFlight> flight;
    bool is_owner = false;
    {
        std::unique_lock<std::mutex> lock(state->mutex);
        const auto ready_it = state->ready.find(key);
        if (ready_it != state->ready.end()) {
            const auto elapsed = observed_now - ready_it->second.loaded_at;
            if (elapsed < ready_it->second.ttl) {
                state->lru.splice(state->lru.begin(), state->lru,
                                  ready_it->second.lru_position);
                ready_it->second.lru_position = state->lru.begin();
                return LoadResult::ok(ready_it->second.value);
            }
            state->lru.erase(ready_it->second.lru_position);
            state->ready.erase(ready_it);
        }

        const auto inflight_it = state->in_flight.find(key);
        if (inflight_it != state->in_flight.end()) {
            flight = inflight_it->second;
            while (!flight->done) {
                flight->completed.wait(lock);
            }
            return flight->result;
        }

        flight = std::make_shared<State::InFlight>();
        state->in_flight.emplace(key, flight);
        is_owner = true;
    }

    if (!is_owner) {
        // The waiting path returns while it holds the state mutex above.
        return LoadResult::failed("unreachable waiting state");
    }

    LoadResult result;
    {
        ActiveLoadScope active_load(state.get(), key);
        try {
            result = loader(key);
        } catch (const std::exception& error) {
            result = LoadResult::failed(error.what());
        } catch (...) {
            result = LoadResult::failed("loader threw an unknown exception");
        }
    }

    std::uint64_t completed_at = 0;
    bool can_cache_result = false;
    if (result.status == LoadStatus::ok && ttl != 0) {
        try {
            // This read is intentionally after the loader returns: TTL starts
            // at loader completion, not when the miss was observed.
            completed_at = state->now();
            can_cache_result = true;
        } catch (...) {
            // The successful load is still delivered to all coalesced callers;
            // an unavailable clock merely prevents caching it.
        }
    }

    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (can_cache_result && flight->cacheable &&
            state->capacity != 0) {
            state->lru.push_front(key);
            State::ReadyValue ready{result.value, completed_at, ttl,
                                    state->lru.begin()};
            state->ready.emplace(key, std::move(ready));

            while (state->ready.size() > state->capacity) {
                const std::string evicted_key = state->lru.back();
                state->lru.pop_back();
                state->ready.erase(evicted_key);
            }
        }

        flight->result = result;
        flight->done = true;
        const auto found = state->in_flight.find(key);
        if (found != state->in_flight.end() && found->second == flight) {
            state->in_flight.erase(found);
        }
    }
    flight->completed.notify_all();
    return result;
}

void CoalescingCache::invalidate(const std::string& key) {
    const auto state = state_;
    std::lock_guard<std::mutex> lock(state->mutex);
    const auto ready_it = state->ready.find(key);
    if (ready_it != state->ready.end()) {
        state->lru.erase(ready_it->second.lru_position);
        state->ready.erase(ready_it);
    }

    const auto inflight_it = state->in_flight.find(key);
    if (inflight_it != state->in_flight.end()) {
        // Existing waiters still observe this flight's result.  The flag only
        // suppresses writing it into the ready cache when it completes.
        inflight_it->second->cacheable = false;
    }
}

std::size_t CoalescingCache::size() const {
    const auto state = state_;
    std::uint64_t observed_now = 0;
    try {
        observed_now = state->now();
    } catch (...) {
        // size() remains a non-throwing inspection operation even if a
        // caller-provided clock is temporarily unavailable.
        std::lock_guard<std::mutex> lock(state->mutex);
        return state->ready.size();
    }

    std::lock_guard<std::mutex> lock(state->mutex);
    for (auto it = state->ready.begin(); it != state->ready.end();) {
        if (observed_now - it->second.loaded_at >= it->second.ttl) {
            state->lru.erase(it->second.lru_position);
            it = state->ready.erase(it);
        } else {
            ++it;
        }
    }
    return state->ready.size();
}
