#include "coalescing_cache.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

int main() {
    {
        std::uint64_t now = 10;
        CoalescingCache cache(2, [&now] { return now; });
        int outer_loads = 0;
        const auto result = cache.get_or_load("self", 5, [&](const std::string&) {
            ++outer_loads;
            const auto inner = cache.get_or_load("self", 5, [](const std::string&) {
                return LoadResult::ok("unexpected");
            });
            assert(inner.status == LoadStatus::recursive_load);
            return LoadResult::ok("outer");
        });

        assert(outer_loads == 1);
        assert(result.status == LoadStatus::ok);
        assert(result.value == "outer");
        assert(cache.size() == 1);
    }

    {
        std::atomic<std::uint64_t> now{100};
        CoalescingCache cache(2, [&now] { return now.load(); });
        std::mutex gate_mutex;
        std::condition_variable gate_ready;
        std::condition_variable gate_release;
        bool started = false;
        bool release = false;
        std::atomic<int> loads{0};
        LoadResult owner_result;
        LoadResult waiter_result;

        std::thread owner([&] {
            owner_result = cache.get_or_load("profile", 10, [&](const std::string&) {
                ++loads;
                {
                    std::lock_guard<std::mutex> lock(gate_mutex);
                    started = true;
                }
                gate_ready.notify_one();

                std::unique_lock<std::mutex> lock(gate_mutex);
                gate_release.wait(lock, [&] { return release; });
                return LoadResult::ok("alice");
            });
        });

        {
            std::unique_lock<std::mutex> lock(gate_mutex);
            gate_ready.wait(lock, [&] { return started; });
        }

        std::thread waiter([&] {
            waiter_result = cache.get_or_load("profile", 10, [](const std::string&) {
                return LoadResult::ok("unexpected");
            });
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        cache.invalidate("profile");
        {
            std::lock_guard<std::mutex> lock(gate_mutex);
            release = true;
        }
        gate_release.notify_one();

        owner.join();
        waiter.join();

        assert(loads == 1);
        assert(owner_result.status == LoadStatus::ok);
        assert(waiter_result.status == LoadStatus::ok);
        assert(owner_result.value == "alice");
        assert(waiter_result.value == "alice");
        assert(cache.size() == 0);

        const auto reloaded = cache.get_or_load("profile", 10, [&](const std::string&) {
            ++loads;
            return LoadResult::ok("bob");
        });
        assert(loads == 2);
        assert(reloaded.status == LoadStatus::ok);
        assert(reloaded.value == "bob");
        assert(cache.size() == 1);
    }

    {
        std::atomic<std::uint64_t> now{200};
        CoalescingCache cache(1, [&now] { return now.load(); });
        std::atomic<int> loads{0};
        LoadResult first;
        LoadResult second;

        std::thread t1([&] {
            first = cache.get_or_load("ttl0", 0, [&](const std::string&) {
                ++loads;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                return LoadResult::ok("v");
            });
        });
        std::thread t2([&] {
            second = cache.get_or_load("ttl0", 0, [&](const std::string&) {
                ++loads;
                return LoadResult::ok("unexpected");
            });
        });
        t1.join();
        t2.join();

        assert(loads == 1);
        assert(first.status == LoadStatus::ok);
        assert(second.status == LoadStatus::ok);
        assert(cache.size() == 0);
    }
}
