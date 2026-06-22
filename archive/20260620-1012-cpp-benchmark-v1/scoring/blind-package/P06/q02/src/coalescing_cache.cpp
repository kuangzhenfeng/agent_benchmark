#include "coalescing_cache.hpp"

#include <condition_variable>
#include <exception>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct ActiveLoadEntry {
    const void* state = nullptr;
    std::string key;
};

std::vector<ActiveLoadEntry>& active_loads() {
    thread_local std::vector<ActiveLoadEntry> loads;
    return loads;
}

bool is_recursive_request(const void* state, const std::string& key) {
    const auto& loads = active_loads();
    for (auto it = loads.rbegin(); it != loads.rend(); ++it) {
        if (it->state == state && it->key == key) {
            return true;
        }
    }
    return false;
}

class ActiveLoadScope {
public:
    ActiveLoadScope(const void* state, std::string key) {
        active_loads().push_back(ActiveLoadEntry{state, std::move(key)});
    }

    ~ActiveLoadScope() {
        active_loads().pop_back();
    }
};

std::string unknown_exception_message(const char* source) {
    return std::string(source) + " threw";
}

LoadResult capture_failure(const char* source) {
    try {
        throw;
    } catch (const std::exception& error) {
        return LoadResult::failed(error.what());
    } catch (...) {
        return LoadResult::failed(unknown_exception_message(source));
    }
}

bool is_ready_expired(std::uint64_t now_tick, std::uint64_t loaded_at,
                      std::uint64_t ttl) {
    if (ttl == 0) {
        return true;
    }
    return now_tick - loaded_at >= ttl;
}

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
    struct ReadyEntry {
        LoadResult result;
        std::uint64_t loaded_at = 0;
        std::uint64_t ttl = 0;
        std::list<std::string>::iterator lru_position;
    };

    struct Flight {
        std::mutex mutex;
        std::condition_variable ready;
        LoadResult result = LoadResult::failed("flight incomplete");
        bool done = false;
        bool invalidated = false;
    };

    explicit State(std::size_t capacity, Clock now)
        : capacity(capacity), now(std::move(now)) {}

    void touch_ready(std::unordered_map<std::string, ReadyEntry>::iterator it) {
        lru.erase(it->second.lru_position);
        lru.push_front(it->first);
        it->second.lru_position = lru.begin();
    }

    void erase_ready(std::unordered_map<std::string, ReadyEntry>::iterator it) {
        lru.erase(it->second.lru_position);
        ready_cache.erase(it);
    }

    void enforce_capacity() {
        while (ready_cache.size() > capacity && !lru.empty()) {
            const std::string key = lru.back();
            lru.pop_back();
            ready_cache.erase(key);
        }
    }

    mutable std::mutex mutex;
    std::size_t capacity;
    Clock now;
    std::unordered_map<std::string, ReadyEntry> ready_cache;
    std::list<std::string> lru;
    std::unordered_map<std::string, std::shared_ptr<Flight>> inflight;
};

CoalescingCache::CoalescingCache(std::size_t capacity, Clock now)
    : state_(std::make_shared<State>(capacity, std::move(now))) {}

LoadResult CoalescingCache::get_or_load(const std::string& key, std::uint64_t ttl,
                                        const Loader& loader) {
    if (is_recursive_request(state_.get(), key)) {
        return LoadResult::recursive();
    }
    ActiveLoadScope active_scope(state_.get(), key);

    for (;;) {
        std::shared_ptr<State::Flight> flight;
        bool owner = false;

        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            auto inflight_it = state_->inflight.find(key);
            if (inflight_it != state_->inflight.end()) {
                flight = inflight_it->second;
            }
        }
        if (flight != nullptr) {
            std::unique_lock<std::mutex> lock(flight->mutex);
            flight->ready.wait(lock, [&flight] { return flight->done; });
            return flight->result;
        }

        std::uint64_t now_tick = 0;
        try {
            now_tick = state_->now();
        } catch (...) {
            return capture_failure("clock");
        }

        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            auto inflight_it = state_->inflight.find(key);
            if (inflight_it != state_->inflight.end()) {
                flight = inflight_it->second;
            } else {
                auto ready_it = state_->ready_cache.find(key);
                if (ready_it != state_->ready_cache.end() &&
                    !is_ready_expired(now_tick, ready_it->second.loaded_at,
                                      ready_it->second.ttl)) {
                    state_->touch_ready(ready_it);
                    return ready_it->second.result;
                }

                if (ready_it != state_->ready_cache.end()) {
                    state_->erase_ready(ready_it);
                }

                flight = std::make_shared<State::Flight>();
                state_->inflight.emplace(key, flight);
                owner = true;
            }
        }
        if (!owner) {
            std::unique_lock<std::mutex> lock(flight->mutex);
            flight->ready.wait(lock, [&flight] { return flight->done; });
            return flight->result;
        }

        LoadResult result;
        try {
            result = loader(key);
        } catch (...) {
            result = capture_failure("loader");
        }

        std::uint64_t completion_tick = 0;
        if (result.status == LoadStatus::ok && ttl > 0 && state_->capacity > 0) {
            try {
                completion_tick = state_->now();
            } catch (...) {
                result = capture_failure("clock");
            }
        }

        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            if (result.status == LoadStatus::ok && ttl > 0 && state_->capacity > 0 &&
                !flight->invalidated) {
                auto ready_it = state_->ready_cache.find(key);
                if (ready_it != state_->ready_cache.end()) {
                    state_->erase_ready(ready_it);
                }
                state_->lru.push_front(key);
                state_->ready_cache.emplace(
                    key, State::ReadyEntry{result, completion_tick, ttl, state_->lru.begin()});
                state_->enforce_capacity();
            }

            {
                std::lock_guard<std::mutex> flight_lock(flight->mutex);
                flight->result = result;
                flight->done = true;
            }
            flight->ready.notify_all();

            auto inflight_it = state_->inflight.find(key);
            if (inflight_it != state_->inflight.end() && inflight_it->second == flight) {
                state_->inflight.erase(inflight_it);
            }
        }

        return result;
    }
}

void CoalescingCache::invalidate(const std::string& key) {
    std::lock_guard<std::mutex> lock(state_->mutex);

    auto ready_it = state_->ready_cache.find(key);
    if (ready_it != state_->ready_cache.end()) {
        state_->erase_ready(ready_it);
    }

    auto inflight_it = state_->inflight.find(key);
    if (inflight_it != state_->inflight.end()) {
        inflight_it->second->invalidated = true;
    }
}

std::size_t CoalescingCache::size() const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->ready_cache.size();
}
