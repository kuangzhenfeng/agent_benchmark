#include "coalescing_cache.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

int main() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(2, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    std::vector<LoadResult> results(6);
    std::vector<std::thread> threads;

    for (std::size_t index = 0; index < results.size(); ++index) {
        threads.emplace_back([&cache, &loads, &results, index] {
            results[index] = cache.get_or_load("profile:7", 10, [&loads](const std::string&) {
                ++loads;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                return LoadResult::ok("alice");
            });
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    assert(loads == 1);
    for (const auto& result : results) {
        assert(result.status == LoadStatus::ok);
        assert(result.value == "alice");
    }
    assert(cache.size() == 1);
}
