#include "coalescing_cache.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

void test_loader_exception() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(2, [&now] { return now.load(); });

    std::atomic<int> loads{0};
    auto result = cache.get_or_load("key1", 10, [&loads](const std::string&) -> LoadResult {
        ++loads;
        throw std::runtime_error("loader error");
    });

    assert(result.status == LoadStatus::loader_failed);
    assert(loads == 1);
    assert(cache.size() == 0);
}

void test_loader_failed_no_cache() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(2, [&now] { return now.load(); });

    int loads = 0;
    auto result = cache.get_or_load("key1", 10, [&loads](const std::string&) {
        ++loads;
        return LoadResult::failed("error");
    });

    assert(result.status == LoadStatus::loader_failed);
    assert(loads == 1);
    assert(cache.size() == 0);

    auto result2 = cache.get_or_load("key1", 10, [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("value");
    });
    assert(result2.status == LoadStatus::ok);
    assert(loads == 2);
}

void test_ttl_expiry_reload() {
    std::uint64_t tick = 100;
    CoalescingCache cache(2, [&tick] { return tick; });

    int loads = 0;
    auto result1 = cache.get_or_load("key1", 10, [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("v1");
    });
    assert(result1.status == LoadStatus::ok);
    assert(loads == 1);
    assert(cache.size() == 1);

    tick = 110;
    auto result2 = cache.get_or_load("key1", 10, [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("v2");
    });
    assert(result2.status == LoadStatus::ok);
    assert(loads == 2);
}

void test_invalidate_removes_ready() {
    std::uint64_t tick = 100;
    CoalescingCache cache(2, [&tick] { return tick; });

    int loads = 0;
    auto result = cache.get_or_load("key1", 1000, [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("value");
    });
    assert(result.status == LoadStatus::ok);
    assert(loads == 1);
    assert(cache.size() == 1);

    cache.invalidate("key1");
    assert(cache.size() == 0);

    auto result2 = cache.get_or_load("key1", 1000, [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("value2");
    });
    assert(result2.status == LoadStatus::ok);
    assert(loads == 2);
}

void test_recursive_load_same_key() {
    CoalescingCache* cache_ptr = nullptr;
    std::uint64_t tick = 100;
    CoalescingCache cache(2, [&tick] { return tick++; });
    cache_ptr = &cache;

    int loads = 0;
    auto result = cache.get_or_load("key1", 10, [&cache_ptr, &loads](const std::string& key) {
        ++loads;
        auto inner = cache_ptr->get_or_load(key, 10, [&loads](const std::string&) {
            ++loads;
            return LoadResult::ok("inner");
        });
        assert(inner.status == LoadStatus::recursive_load);
        return LoadResult::ok("value");
    });

    assert(result.status == LoadStatus::ok);
    assert(loads == 1);
}

void test_capacity_zero_no_cache() {
    std::uint64_t tick = 100;
    CoalescingCache cache(0, [&tick] { return tick++; });

    int loads = 0;
    auto result = cache.get_or_load("key1", 10, [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("value");
    });
    assert(result.status == LoadStatus::ok);
    assert(loads == 1);
    assert(cache.size() == 0);

    auto result2 = cache.get_or_load("key1", 10, [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("value2");
    });
    assert(result2.status == LoadStatus::ok);
    assert(loads == 2);
    assert(cache.size() == 0);
}

void test_lru_eviction() {
    std::uint64_t tick = 100;
    CoalescingCache cache(2, [&tick] { return tick++; });

    auto r1 = cache.get_or_load("k1", 1000, [](const std::string&) { return LoadResult::ok("v1"); });
    auto r2 = cache.get_or_load("k2", 1000, [](const std::string&) { return LoadResult::ok("v2"); });
    assert(cache.size() == 2);

    auto r3 = cache.get_or_load("k3", 1000, [](const std::string&) { return LoadResult::ok("v3"); });
    assert(cache.size() == 2);

    auto r1_again = cache.get_or_load("k1", 1000, [](const std::string&) { return LoadResult::ok("v1_new"); });
    assert(r1_again.status == LoadStatus::ok);
}

void test_concurrent_loader_exception() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(2, [&now] { return now.load(); });
    std::atomic<int> loads{0};

    std::vector<LoadResult> results(4);
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < results.size(); ++i) {
        threads.emplace_back([&cache, &loads, &results, i] {
            results[i] = cache.get_or_load("key1", 10, [&loads](const std::string&) -> LoadResult {
                ++loads;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                throw std::runtime_error("fail");
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

int main() {
    test_loader_exception();
    test_loader_failed_no_cache();
    test_ttl_expiry_reload();
    test_invalidate_removes_ready();
    test_recursive_load_same_key();
    test_capacity_zero_no_cache();
    test_lru_eviction();
    test_concurrent_loader_exception();
    return 0;
}
