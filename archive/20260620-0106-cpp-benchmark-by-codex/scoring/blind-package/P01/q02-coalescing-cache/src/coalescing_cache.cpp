#include "coalescing_cache.hpp"

#include <condition_variable>
#include <exception>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>
#include <future>

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

struct ReadyEntry {
    std::string value;
    std::uint64_t expiry;
};

using ReadyList = std::list<std::pair<std::string, ReadyEntry>>;
using ReadyMap = std::unordered_map<std::string, ReadyList::iterator>;

struct InflightState {
    std::shared_ptr<std::promise<LoadResult>> promise;
    std::shared_future<LoadResult> future;
    bool invalidated = false;
    std::thread::id loader_thread;
};

using InflightMap = std::unordered_map<std::string, InflightState>;

} // namespace

struct CoalescingCache::State {
    explicit State(std::size_t cap, Clock fn)
        : capacity(cap), now(std::move(fn)) {}

    std::size_t capacity;
    Clock now;

    std::mutex mutex;
    std::condition_variable cv;

    ReadyList ready_list;
    ReadyMap ready_map;
    InflightMap inflight_map;
};

CoalescingCache::CoalescingCache(std::size_t capacity, Clock now)
    : state_(std::make_shared<State>(capacity, std::move(now))) {}

LoadResult CoalescingCache::get_or_load(const std::string& key, std::uint64_t ttl,
                                         const Loader& loader) {
    std::unique_lock<std::mutex> lock(state_->mutex);

    auto ready_it = state_->ready_map.find(key);
    if (ready_it != state_->ready_map.end()) {
        auto list_it = ready_it->second;
        const std::uint64_t now_val = state_->now();
        if (now_val < list_it->second.expiry) {
            state_->ready_list.splice(state_->ready_list.begin(),
                                       state_->ready_list, list_it);
            LoadResult result = LoadResult::ok(list_it->second.value);
            lock.unlock();
            return result;
        }
        state_->ready_list.erase(list_it);
        state_->ready_map.erase(ready_it);
    }

    auto inflight_it = state_->inflight_map.find(key);
    if (inflight_it != state_->inflight_map.end()) {
        if (inflight_it->second.loader_thread == std::this_thread::get_id()) {
            lock.unlock();
            return LoadResult::recursive();
        }
        auto fut = inflight_it->second.future;
        lock.unlock();
        return fut.get();
    }

    auto prom = std::make_shared<std::promise<LoadResult>>();
    auto fut = prom->get_future().share();
    InflightState is;
    is.promise = prom;
    is.future = fut;
    is.invalidated = false;
    is.loader_thread = std::this_thread::get_id();
    state_->inflight_map.emplace(key, std::move(is));

    lock.unlock();

    LoadResult result;
    try {
        result = loader(key);
    } catch (const std::exception& ex) {
        result = LoadResult::failed(ex.what());
    } catch (...) {
        result = LoadResult::failed("unknown exception in loader");
    }

    lock.lock();

    auto my_inflight = state_->inflight_map.find(key);
    if (my_inflight == state_->inflight_map.end()) {
        lock.unlock();
        return result;
    }

    auto& entry = my_inflight->second;
    bool was_invalidated = entry.invalidated;

    try {
        entry.promise->set_value(result);
    } catch (...) {
    }

    state_->inflight_map.erase(my_inflight);

    if (result.status == LoadStatus::ok && !was_invalidated &&
        state_->capacity > 0 && ttl > 0) {
        const std::uint64_t now_val = state_->now();
        const std::uint64_t expiry = now_val + ttl;

        while (state_->ready_map.size() >= state_->capacity) {
            auto back = state_->ready_list.end();
            --back;
            state_->ready_map.erase(back->first);
            state_->ready_list.erase(back);
        }
        state_->ready_list.emplace_front(key, ReadyEntry{result.value, expiry});
        state_->ready_map[key] = state_->ready_list.begin();
    }

    lock.unlock();
    state_->cv.notify_all();

    return result;
}

void CoalescingCache::invalidate(const std::string& key) {
    std::lock_guard<std::mutex> lock(state_->mutex);

    auto ready_it = state_->ready_map.find(key);
    if (ready_it != state_->ready_map.end()) {
        state_->ready_list.erase(ready_it->second);
        state_->ready_map.erase(ready_it);
    }

    auto inflight_it = state_->inflight_map.find(key);
    if (inflight_it != state_->inflight_map.end()) {
        inflight_it->second.invalidated = true;
    }
}

std::size_t CoalescingCache::size() const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->ready_map.size();
}
