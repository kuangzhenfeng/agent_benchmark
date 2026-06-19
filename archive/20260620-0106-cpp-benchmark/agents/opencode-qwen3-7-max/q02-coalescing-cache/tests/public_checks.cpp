#include "coalescing_cache.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

int main() {
    {
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
        for (const auto& r : results) {
            assert(r.status == LoadStatus::ok);
            assert(r.value == "alice");
        }
        assert(cache.size() == 1);
    }

    {
        std::atomic<std::uint64_t> now{100};
        CoalescingCache cache(2, [&now] { return now.load(); });
        cache.get_or_load("k1", 10, [](const std::string&) { return LoadResult::ok("v1"); });
        auto r = cache.get_or_load("k1", 10, [](const std::string&) { return LoadResult::ok("v1-new"); });
        assert(r.status == LoadStatus::ok && r.value == "v1");
    }

    {
        std::atomic<std::uint64_t> now{100};
        CoalescingCache cache(2, [&now] { return now.load(); });
        cache.get_or_load("k1", 10, [](const std::string&) { return LoadResult::ok("v1"); });
        now.store(115);
        auto r = cache.get_or_load("k1", 10, [](const std::string&) { return LoadResult::ok("v1-reloaded"); });
        assert(r.status == LoadStatus::ok && r.value == "v1-reloaded");
    }

    {
        std::atomic<std::uint64_t> now{100};
        CoalescingCache cache(2, [&now] { return now.load(); });
        std::atomic<int> loads{0};
        std::vector<LoadResult> results(4);
        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&, i] {
                results[i] = cache.get_or_load("fail-key", 10, [&loads](const std::string&) {
                    ++loads;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    return LoadResult::failed("backend down");
                });
            });
        }
        for (auto& t : threads) t.join();
        assert(loads == 1);
        for (const auto& r : results) {
            assert(r.status == LoadStatus::loader_failed);
        }
        assert(cache.size() == 0);
    }

    {
        std::atomic<std::uint64_t> now{100};
        CoalescingCache cache(2, [&now] { return now.load(); });
        std::atomic<int> loads{0};
        std::vector<LoadResult> results(3);
        std::vector<std::thread> threads;
        for (int i = 0; i < 3; ++i) {
            threads.emplace_back([&, i] {
                results[i] = cache.get_or_load("ex-key", 10, [&loads](const std::string&) -> LoadResult {
                    ++loads;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    throw std::runtime_error("oops");
                });
            });
        }
        for (auto& t : threads) t.join();
        assert(loads == 1);
        for (const auto& r : results) {
            assert(r.status == LoadStatus::loader_failed);
        }
    }

    {
        std::atomic<std::uint64_t> now{100};
        CoalescingCache cache(2, [&now] { return now.load(); });
        cache.get_or_load("a", 10, [](const std::string&) { return LoadResult::ok("va"); });
        cache.get_or_load("b", 10, [](const std::string&) { return LoadResult::ok("vb"); });
        assert(cache.size() == 2);
        cache.invalidate("a");
        assert(cache.size() == 1);
        auto r = cache.get_or_load("a", 10, [](const std::string&) { return LoadResult::ok("va-new"); });
        assert(r.value == "va-new");
    }

    {
        std::atomic<std::uint64_t> now{100};
        CoalescingCache cache(2, [&now] { return now.load(); });
        cache.get_or_load("a", 10, [](const std::string&) { return LoadResult::ok("va"); });
        cache.get_or_load("b", 10, [](const std::string&) { return LoadResult::ok("vb"); });
        auto r = cache.get_or_load("c", 10, [](const std::string&) { return LoadResult::ok("vc"); });
        assert(cache.size() == 2);
        assert(r.value == "vc");
    }

    {
        std::atomic<std::uint64_t> now{100};
        CoalescingCache cache(0, [&now] { return now.load(); });
        std::atomic<int> loads{0};
        cache.get_or_load("k", 10, [&loads](const std::string&) { ++loads; return LoadResult::ok("v"); });
        assert(cache.size() == 0);
        cache.get_or_load("k", 10, [&loads](const std::string&) { ++loads; return LoadResult::ok("v2"); });
        assert(loads == 2);
    }

    {
        std::atomic<std::uint64_t> now{100};
        CoalescingCache cache(2, [&now] { return now.load(); });
        auto r = cache.get_or_load("k", 0, [](const std::string&) { return LoadResult::ok("v"); });
        assert(r.status == LoadStatus::ok);
        assert(cache.size() == 0);
        std::atomic<int> loads{0};
        r = cache.get_or_load("k", 0, [&loads](const std::string&) { ++loads; return LoadResult::ok("v2"); });
        assert(loads == 1);
    }

    {
        std::atomic<std::uint64_t> now{100};
        CoalescingCache cache(4, [&now] { return now.load(); });
        CoalescingCache::Loader outer_loader = [&cache](const std::string&) -> LoadResult {
            LoadResult inner = cache.get_or_load("outer", 10,
                CoalescingCache::Loader([](const std::string&) -> LoadResult {
                    return LoadResult::ok("should-not-be-called");
                }));
            assert(inner.status == LoadStatus::recursive_load);
            LoadResult other = cache.get_or_load("different", 10,
                CoalescingCache::Loader([](const std::string&) -> LoadResult {
                    return LoadResult::ok("ok-different");
                }));
            assert(other.status == LoadStatus::ok && other.value == "ok-different");
            return LoadResult::ok("outer-result");
        };
        LoadResult r = cache.get_or_load("outer", 10, outer_loader);
        assert(r.status == LoadStatus::ok && r.value == "outer-result");
    }

    {
        std::atomic<std::uint64_t> now{100};
        CoalescingCache cache(2, [&now] { return now.load(); });
        std::atomic<bool> invalidate_called{false};
        std::atomic<bool> loader_done{false};

        std::thread loader_thread([&] {
            cache.get_or_load("k", 10, [&](const std::string&) {
                while (!invalidate_called.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                loader_done.store(true);
                return LoadResult::ok("v");
            });
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        cache.invalidate("k");
        invalidate_called.store(true);
        loader_thread.join();

        assert(cache.size() == 0);
        auto r = cache.get_or_load("k", 10, [](const std::string&) { return LoadResult::ok("v2"); });
        assert(r.value == "v2");
    }

    return 0;
}
