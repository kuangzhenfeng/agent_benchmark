#include "coalescing_cache.hpp"

#include <condition_variable>
#include <cstdint>
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

// All shared state lives behind a single mutex. The in-flight table, the ready
// cache, AND each flight's done/result are all synchronized through this one
// mutex — so the "single synchronizable state transition" of a flight completing
// (decide result, decide cache write, publish to waiters, then make the flight
// undiscoverable) all happen under the same lock. The user loader and the user
// Clock are only ever invoked *outside* this mutex.
struct CoalescingCache::State {
    explicit State(std::size_t capacity, Clock now)
        : capacity(capacity), now(std::move(now)) {}

    struct Entry {
        std::string key;
        std::string value;
        std::uint64_t ttl = 0;
        std::uint64_t loaded_at = 0;
    };

    struct Flight {
        std::thread::id owner;            // who is running the loader
        std::condition_variable cv;       // paired with State::mutex
        bool done = false;
        bool invalidated = false;         // invalidate() raced with this flight
        LoadResult result;
    };

    const std::size_t capacity;
    Clock now;
    mutable std::mutex mutex;
    std::list<Entry> entries;                                   // front = most-recently-used
    std::unordered_map<std::string, std::list<Entry>::iterator> index;
    std::unordered_map<std::string, std::shared_ptr<Flight>> inflight;
};

CoalescingCache::CoalescingCache(std::size_t capacity, Clock now)
    : state_(std::make_shared<State>(capacity, std::move(now))) {}

LoadResult CoalescingCache::get_or_load(const std::string& key,
                                        std::uint64_t ttl,
                                        const Loader& loader) {
    State& s = *state_;

    for (;;) {
        std::shared_ptr<State::Flight> flight;
        bool coalesce = false;
        LoadResult coalesced;

        // ---- Decide: serve ready, coalesce onto a flight, or become owner ----
        {
            std::unique_lock<std::mutex> lock(s.mutex);

            auto fit = s.inflight.find(key);
            if (fit != s.inflight.end()) {
                flight = fit->second;
                if (flight->owner == std::this_thread::get_id()) {
                    // Same-key re-entry by the owner: must not wait for itself.
                    return LoadResult::recursive();
                }
                // Coalesce: wait for the owner to finish, under the shared mutex.
                flight->cv.wait(lock, [&] { return flight->done; });
                coalesced = flight->result;        // read result under lock
                coalesce = true;
            } else {
                auto rit = s.index.find(key);
                if (rit != s.index.end()) {
                    const std::uint64_t loaded_at = rit->second->loaded_at;
                    const std::uint64_t entry_ttl = rit->second->ttl;
                    // Clock must run outside the lock — release first.
                    lock.unlock();
                    std::uint64_t now = 0;
                    try {
                        now = s.now();             // user Clock, may re-enter
                    } catch (...) {
                        now = loaded_at;           // treat as not-expired on clock error
                    }
                    const bool expired = (now - loaded_at) >= entry_ttl;
                    if (!expired) {
                        std::unique_lock<std::mutex> lock2(s.mutex);
                        auto rit2 = s.index.find(key);
                        if (rit2 != s.index.end() &&
                            rit2->second->loaded_at == loaded_at) {
                            s.entries.splice(s.entries.begin(), s.entries,
                                             rit2->second);  // refresh LRU position
                            return LoadResult::ok(rit2->second->value);
                        }
                        continue;  // entry was replaced concurrently; retry
                    }
                    // Expired: evict (if still the same entry) then become owner.
                    std::unique_lock<std::mutex> lock3(s.mutex);
                    auto rit3 = s.index.find(key);
                    if (rit3 != s.index.end()) {
                        if (rit3->second->loaded_at != loaded_at) {
                            continue;  // another loader already refreshed it
                        }
                        s.entries.erase(rit3->second);
                        s.index.erase(rit3);
                    }
                    flight = std::make_shared<State::Flight>();
                    flight->owner = std::this_thread::get_id();
                    s.inflight.emplace(key, flight);
                } else {
                    // Nothing ready, nothing in-flight: become the owner.
                    flight = std::make_shared<State::Flight>();
                    flight->owner = std::this_thread::get_id();
                    s.inflight.emplace(key, flight);
                }
            }
        }  // mutex released

        if (coalesce) {
            return coalesced;
        }

        // ---- We are the owner. Run the user loader OUTSIDE the lock. ----
        LoadResult result;
        bool ok = false;
        try {
            result = loader(key);
            ok = (result.status == LoadStatus::ok);
        } catch (...) {
            result = LoadResult::failed("loader threw");
            ok = false;
        }

        // TTL anchor = clock value at loader completion time; outside the lock.
        std::uint64_t loaded_at = 0;
        if (ok) {
            try {
                loaded_at = s.now();
            } catch (...) {
                loaded_at = 0;
            }
        }

        const bool cacheable = ok && ttl > 0 && s.capacity > 0;

        // ---- Complete the flight: one atomic state transition. ----
        {
            std::unique_lock<std::mutex> lock(s.mutex);
            if (cacheable && !flight->invalidated) {
                auto existing = s.index.find(key);
                if (existing != s.index.end()) {
                    existing->second->value = result.value;
                    existing->second->ttl = ttl;
                    existing->second->loaded_at = loaded_at;
                    s.entries.splice(s.entries.begin(), s.entries, existing->second);
                } else {
                    if (s.entries.size() >= s.capacity) {
                        auto victim = std::prev(s.entries.end());  // LRU = back
                        s.index.erase(victim->key);
                        s.entries.pop_back();
                    }
                    s.entries.push_front(
                        State::Entry{key, result.value, ttl, loaded_at});
                    s.index[key] = s.entries.begin();
                }
            }
            // Publish the result, then make the flight undiscoverable — in that
            // order, under this same lock, so any finder observes a complete
            // result through the same synchronization.
            flight->result = result;
            flight->done = true;
            s.inflight.erase(key);
        }
        flight->cv.notify_all();  // wake coalesced waiters outside the lock
        return result;
    }
}

void CoalescingCache::invalidate(const std::string& key) {
    State& s = *state_;
    std::unique_lock<std::mutex> lock(s.mutex);
    auto rit = s.index.find(key);
    if (rit != s.index.end()) {
        s.entries.erase(rit->second);
        s.index.erase(rit);
    }
    auto fit = s.inflight.find(key);
    if (fit != s.inflight.end()) {
        // The in-flight loader keeps running; existing waiters still get its
        // result, but the owner will NOT write it back to the ready cache.
        fit->second->invalidated = true;
    }
}

std::size_t CoalescingCache::size() const {
    State& s = *state_;
    std::unique_lock<std::mutex> lock(s.mutex);
    return s.entries.size();
}
