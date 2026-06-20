#include "coalescing_cache.hpp"

#include <condition_variable>
#include <limits>
#include <map>
#include <mutex>
#include <thread>
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
    explicit State(std::size_t capacity, Clock now_fn)
        : capacity(capacity), now_fn(std::move(now_fn)) {}

    std::size_t capacity;
    Clock now_fn;
    std::mutex mutex;
    std::condition_variable cv;
    std::uint64_t next_lru_seq = 0;

    struct ReadyEntry {
        std::string value;
        std::uint64_t expiry_time;
        std::uint64_t lru_seq;
    };

    std::map<std::string, ReadyEntry> ready;

    struct Flight {
        bool done = false;
        LoadResult result;
        bool invalidated = false;
        std::thread::id owner_thread;
    };

    std::map<std::string, std::shared_ptr<Flight>> inflight;
};

CoalescingCache::CoalescingCache(std::size_t capacity, Clock now)
    : state_(std::make_shared<State>(capacity, std::move(now))) {}

LoadResult CoalescingCache::get_or_load(const std::string& key, std::uint64_t ttl,
                                         const Loader& loader) {
    std::uint64_t current_time = state_->now_fn();

    std::shared_ptr<State::Flight> flight;
    bool is_owner = false;

    {
        std::lock_guard<std::mutex> lock(state_->mutex);

        auto ready_it = state_->ready.find(key);
        if (ready_it != state_->ready.end()) {
            if (current_time < ready_it->second.expiry_time) {
                ready_it->second.lru_seq = state_->next_lru_seq++;
                return LoadResult::ok(ready_it->second.value);
            }
            state_->ready.erase(ready_it);
        }

        auto inflight_it = state_->inflight.find(key);
        if (inflight_it != state_->inflight.end()) {
            flight = inflight_it->second;
            if (flight->owner_thread == std::this_thread::get_id()) {
                return LoadResult::recursive();
            }
            is_owner = false;
        } else {
            flight = std::make_shared<State::Flight>();
            flight->owner_thread = std::this_thread::get_id();
            state_->inflight[key] = flight;
            is_owner = true;
        }
    }

    if (!is_owner) {
        LoadResult result;
        {
            std::unique_lock<std::mutex> ulock(state_->mutex);
            while (!flight->done) {
                state_->cv.wait(ulock);
            }
            result = flight->result;
        }
        return result;
    }

    LoadResult result;
    try {
        result = loader(key);
    } catch (...) {
        result = LoadResult::failed("loader threw exception");
    }

    std::uint64_t load_time = state_->now_fn();

    {
        std::lock_guard<std::mutex> lock(state_->mutex);

        bool should_cache =
            result.status == LoadStatus::ok &&
            ttl > 0 &&
            state_->capacity > 0 &&
            !flight->invalidated;

        if (should_cache) {
            while (state_->ready.size() >= state_->capacity) {
                auto lru_it = state_->ready.begin();
                std::uint64_t min_seq = lru_it->second.lru_seq;
                for (auto it = state_->ready.begin(); it != state_->ready.end(); ++it) {
                    if (it->second.lru_seq < min_seq) {
                        min_seq = it->second.lru_seq;
                        lru_it = it;
                    }
                }
                state_->ready.erase(lru_it);
            }
            state_->ready[key] = {result.value, load_time + ttl, state_->next_lru_seq++};
        }

        flight->result = result;
        flight->done = true;
        state_->inflight.erase(key);
        state_->cv.notify_all();
    }

    return result;
}

void CoalescingCache::invalidate(const std::string& key) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->ready.erase(key);
    auto inflight_it = state_->inflight.find(key);
    if (inflight_it != state_->inflight.end()) {
        inflight_it->second->invalidated = true;
    }
}

std::size_t CoalescingCache::size() const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->ready.size();
}
