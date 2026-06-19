#include "coalescing_cache.hpp"

#include <future>
#include <list>
#include <mutex>
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
        std::uint64_t expiry = 0;
        std::list<std::string>::iterator lru_iter;
    };

    struct InFlight {
        std::promise<LoadResult> promise;
        std::shared_future<LoadResult> future;
        std::thread::id loader_thread{};
        bool write_to_ready = true;

        InFlight() : future(promise.get_future().share()) {}
    };

    explicit State(std::size_t capacity, Clock now)
        : capacity(capacity), now(std::move(now)) {}

    std::size_t capacity;
    Clock now;
    mutable std::mutex mutex;
    std::unordered_map<std::string, ReadyEntry> ready;
    std::list<std::string> lru;
    std::unordered_map<std::string, std::shared_ptr<InFlight>> inflight;

    void touch_ready(const std::string& key, ReadyEntry& entry) {
        lru.erase(entry.lru_iter);
        lru.push_front(key);
        entry.lru_iter = lru.begin();
    }

    void evict_until_fit() {
        while (capacity > 0 && ready.size() >= capacity && !lru.empty()) {
            const std::string victim = lru.back();
            ready.erase(victim);
            lru.pop_back();
        }
    }
};

CoalescingCache::CoalescingCache(std::size_t capacity, Clock now)
    : state_(std::make_shared<State>(capacity, std::move(now))) {}

LoadResult CoalescingCache::get_or_load(const std::string& key, std::uint64_t ttl,
                                        const Loader& loader) {
    auto& s = *state_;
    std::shared_future<LoadResult> wait_future;
    bool am_loader = false;

    {
        std::lock_guard<std::mutex> lk(s.mutex);
        const std::uint64_t current = s.now();

        auto rit = s.ready.find(key);
        if (rit != s.ready.end()) {
            if (current < rit->second.expiry) {
                const std::string value = rit->second.value;
                s.touch_ready(key, rit->second);
                return LoadResult::ok(value);
            }
            s.lru.erase(rit->second.lru_iter);
            s.ready.erase(rit);
        }

        auto iit = s.inflight.find(key);
        if (iit != s.inflight.end()) {
            if (iit->second->loader_thread == std::this_thread::get_id()) {
                return LoadResult::recursive();
            }
            wait_future = iit->second->future;
        } else {
            auto inflight = std::make_shared<State::InFlight>();
            inflight->loader_thread = std::this_thread::get_id();
            s.inflight[key] = inflight;
            am_loader = true;
        }
    }

    if (!am_loader) {
        return wait_future.get();
    }

    LoadResult result = LoadResult::failed("loader not invoked");
    try {
        if (loader) {
            result = loader(key);
        } else {
            result = LoadResult::failed("null loader");
        }
    } catch (...) {
        result = LoadResult::failed("loader threw exception");
    }

    std::shared_ptr<State::InFlight> inflight_holder;
    {
        std::lock_guard<std::mutex> lk(s.mutex);
        auto iit = s.inflight.find(key);
        if (iit != s.inflight.end()) {
            inflight_holder = iit->second;
            if (result.status == LoadStatus::ok &&
                inflight_holder->write_to_ready &&
                ttl > 0 && s.capacity > 0) {
                const std::uint64_t completion_time = s.now();
                s.evict_until_fit();
                State::ReadyEntry entry;
                entry.value = result.value;
                entry.expiry = completion_time + ttl;
                s.lru.push_front(key);
                entry.lru_iter = s.lru.begin();
                s.ready[key] = std::move(entry);
            }
            s.inflight.erase(iit);
        }
    }

    if (inflight_holder) {
        try {
            inflight_holder->promise.set_value(result);
        } catch (...) {
        }
    }

    return result;
}

void CoalescingCache::invalidate(const std::string& key) {
    auto& s = *state_;
    std::lock_guard<std::mutex> lk(s.mutex);
    auto rit = s.ready.find(key);
    if (rit != s.ready.end()) {
        s.lru.erase(rit->second.lru_iter);
        s.ready.erase(rit);
    }
    auto iit = s.inflight.find(key);
    if (iit != s.inflight.end()) {
        iit->second->write_to_ready = false;
    }
}

std::size_t CoalescingCache::size() const {
    auto& s = *state_;
    std::lock_guard<std::mutex> lk(s.mutex);
    return s.ready.size();
}
