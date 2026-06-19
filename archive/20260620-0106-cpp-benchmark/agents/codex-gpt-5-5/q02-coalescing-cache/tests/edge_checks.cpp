#include "coalescing_cache.hpp"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

int main() {
    {
        std::atomic<std::uint64_t> now{0};
        CoalescingCache cache(2, [&now] { return now.load(); });
        int loads = 0;
        const auto first = cache.get_or_load("key", 10, [&loads, &now](const std::string&) {
            ++loads;
            now.store(10);
            return LoadResult::ok("first");
        });
        assert(first.status == LoadStatus::ok);
        now.store(19);
        assert(cache.get_or_load("key", 10, [&loads](const std::string&) {
                   ++loads;
                   return LoadResult::ok("wrong");
               }).value == "first");
        now.store(20);
        assert(cache.get_or_load("key", 10, [&loads](const std::string&) {
                   ++loads;
                   return LoadResult::ok("second");
               }).value == "second");
        assert(loads == 2);
    }

    {
        CoalescingCache cache(2, [] { return std::uint64_t{0}; });
        bool saw_recursive = false;
        bool loaded_other_key = false;
        const auto result = cache.get_or_load("self", 1, [&cache, &saw_recursive, &loaded_other_key](const std::string&) {
            const auto other = cache.get_or_load("other", 1, [](const std::string&) {
                return LoadResult::ok("other");
            });
            loaded_other_key = other.status == LoadStatus::ok && other.value == "other";
            const auto nested = cache.get_or_load("self", 1, [](const std::string&) {
                return LoadResult::ok("unreachable");
            });
            saw_recursive = nested.status == LoadStatus::recursive_load;
            return LoadResult::ok("outer");
        });
        assert(result.status == LoadStatus::ok);
        assert(loaded_other_key);
        assert(saw_recursive);
    }

    {
        CoalescingCache cache(2, [] { return std::uint64_t{0}; });
        int loads = 0;
        const auto load = [&loads](const std::string& key) {
            ++loads;
            return LoadResult::ok(key + ":value");
        };
        assert(cache.get_or_load("a", 10, load).status == LoadStatus::ok);
        assert(cache.get_or_load("b", 10, load).status == LoadStatus::ok);
        assert(cache.get_or_load("a", 10, load).status == LoadStatus::ok);
        assert(cache.get_or_load("c", 10, load).status == LoadStatus::ok);
        assert(cache.size() == 2);
        assert(cache.get_or_load("b", 10, load).status == LoadStatus::ok);
        assert(loads == 4);
    }

    {
        CoalescingCache cache(2, [] { return std::uint64_t{0}; });
        std::mutex barrier_mutex;
        std::condition_variable barrier;
        bool entered = false;
        bool release = false;
        std::atomic<int> loads{0};
        LoadResult owner;
        auto loader = [&barrier_mutex, &barrier, &entered, &release, &loads](const std::string&) {
            ++loads;
            std::unique_lock<std::mutex> lock(barrier_mutex);
            entered = true;
            barrier.notify_all();
            barrier.wait(lock, [&release] { return release; });
            return LoadResult::ok("value");
        };
        std::thread owner_thread([&] { owner = cache.get_or_load("key", 10, loader); });
        {
            std::unique_lock<std::mutex> lock(barrier_mutex);
            barrier.wait(lock, [&entered] { return entered; });
        }
        cache.invalidate("key");
        {
            std::lock_guard<std::mutex> lock(barrier_mutex);
            release = true;
        }
        barrier.notify_all();
        owner_thread.join();
        assert(loads == 1);
        assert(owner.status == LoadStatus::ok);
        assert(cache.size() == 0);
    }
}
