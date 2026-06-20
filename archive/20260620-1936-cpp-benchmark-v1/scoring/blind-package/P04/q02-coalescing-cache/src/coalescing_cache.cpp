#include "coalescing_cache.hpp"

#include <list>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

bool is_expired(std::uint64_t now, std::uint64_t expire_tick) {
    return now >= expire_tick;
}

}  // namespace

struct CoalescingCache::State {
    explicit State(std::size_t capacity, Clock now)
        : capacity(capacity), now(std::move(now)) {}

    std::size_t capacity;
    Clock now;

    struct ReadyEntry {
        LoadResult result;
        std::uint64_t expire_tick;
        std::list<std::string>::iterator lru_iter;
    };

    struct Flight {
        std::mutex result_mutex;
        LoadResult result;
        bool done = false;
        std::condition_variable cv;
        std::vector<std::shared_ptr<bool>> wait_flags;
    };

    struct FlightEntry {
        std::shared_ptr<Flight> flight;
    };

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, ReadyEntry> ready_cache;
    std::list<std::string> lru_order;
    std::unordered_map<std::string, FlightEntry> in_flight;
};

LoadResult LoadResult::ok(std::string value) {
    return LoadResult{LoadStatus::ok, std::move(value), {}};
}

LoadResult LoadResult::failed(std::string error) {
    return LoadResult{LoadStatus::loader_failed, {}, std::move(error)};
}

LoadResult LoadResult::recursive() {
    return LoadResult{LoadStatus::recursive_load, {}, "recursive load"};
}

CoalescingCache::CoalescingCache(std::size_t capacity, Clock now)
    : state_(std::make_shared<State>(capacity, std::move(now))) {}

LoadResult CoalescingCache::get_or_load(const std::string& key,
                                         std::uint64_t ttl,
                                         const Loader& loader) {
    std::uint64_t now_val = state_->now();

    {
        std::shared_lock<std::shared_mutex> read_lock(state_->mutex_);

        auto ready_it = state_->ready_cache.find(key);
        if (ready_it != state_->ready_cache.end()) {
            if (!is_expired(now_val, ready_it->second.expire_tick)) {
                state_->lru_order.erase(ready_it->second.lru_iter);
                state_->lru_order.push_front(key);
                ready_it->second.lru_iter = state_->lru_order.begin();
                return ready_it->second.result;
            }
        }

        auto flight_it = state_->in_flight.find(key);
        if (flight_it != state_->in_flight.end()) {
            read_lock.unlock();

            auto flight = flight_it->second.flight;
            std::unique_lock<std::mutex> flight_lock(flight->result_mutex);

            if (!flight->done) {
                auto flag = std::make_shared<bool>(true);
                flight->wait_flags.push_back(flag);

                flight->cv.wait(flight_lock, [flight, flag]() {
                    return flight->done || !(*flag);
                });
            }

            if (flight->done) {
                return flight->result;
            }

            return LoadResult::recursive();
        }
    }

    std::shared_ptr<State::Flight> flight;

    {
        std::unique_lock<std::shared_mutex> write_lock(state_->mutex_);
        flight = std::make_shared<State::Flight>();
        state_->in_flight[key] = {flight};
    }

    LoadResult load_result;
    bool should_cache = (ttl > 0);

    try {
        load_result = loader(key);
    } catch (...) {
        load_result = LoadResult::failed("loader threw exception");
    }

    {
        std::unique_lock<std::mutex> flight_lock(flight->result_mutex);
        flight->result = load_result;
        flight->done = true;

        for (auto& flag : flight->wait_flags) {
            *flag = false;
        }
        flight->cv.notify_all();
    }

    std::uint64_t expire_time = state_->now();

    {
        std::unique_lock<std::shared_mutex> write_lock(state_->mutex_);

        state_->in_flight.erase(key);

        if (load_result.status == LoadStatus::ok && should_cache) {
            if (state_->capacity > 0) {
                while (state_->ready_cache.size() >= state_->capacity) {
                    if (state_->lru_order.empty()) {
                        break;
                    }
                    auto lru_key = state_->lru_order.back();
                    state_->lru_order.pop_back();
                    state_->ready_cache.erase(lru_key);
                }

                state_->lru_order.push_front(key);
                state_->ready_cache[key] = {load_result, expire_time + ttl,
                                             state_->lru_order.begin()};
            }
        }
    }

    return load_result;
}

void CoalescingCache::invalidate(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(state_->mutex_);
    auto it = state_->ready_cache.find(key);
    if (it != state_->ready_cache.end()) {
        state_->lru_order.erase(it->second.lru_iter);
        state_->ready_cache.erase(it);
    }
}

std::size_t CoalescingCache::size() const {
    std::shared_lock<std::shared_mutex> lock(state_->mutex_);
    return state_->ready_cache.size();
}
