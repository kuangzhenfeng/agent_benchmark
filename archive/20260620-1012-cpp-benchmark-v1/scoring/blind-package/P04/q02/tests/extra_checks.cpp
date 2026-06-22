#include "coalescing_cache.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

void test_ttl_expiry_reload() {
    std::atomic<std::uint64_t> now{0};
    CoalescingCache cache(4, [&now] { return now.load(); });
    int loads = 0;
    auto loader = [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("v");
    };
    now = 100;
    auto r1 = cache.get_or_load("k", 10, loader);
    assert(r1.status == LoadStatus::ok && loads == 1);
    assert(cache.size() == 1);
    // Same tick window: cached.
    now = 109;
    auto r2 = cache.get_or_load("k", 10, loader);
    assert(r2.status == LoadStatus::ok && loads == 1);
    // Expired: reload.
    now = 110;
    auto r3 = cache.get_or_load("k", 10, loader);
    assert(r3.status == LoadStatus::ok && loads == 2);
}

void test_ttl_zero_never_caches_but_coalesces() {
    std::atomic<std::uint64_t> now{5};
    CoalescingCache cache(4, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    std::vector<LoadResult> results(8);
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < results.size(); ++i) {
        threads.emplace_back([&cache, &loads, &results, i] {
            results[i] = cache.get_or_load(
                "k", 0, [&loads](const std::string&) {
                    ++loads;
                    std::this_thread::sleep_for(std::chrono::milliseconds(15));
                    return LoadResult::ok("x");
                });
        });
    }
    for (auto& t : threads) t.join();
    assert(loads == 1);  // coalesced into a single load
    assert(cache.size() == 0);  // ttl 0 never cached
    for (const auto& r : results) {
        assert(r.status == LoadStatus::ok && r.value == "x");
    }
    // Next call triggers a fresh load.
    int loads2 = 0;
    auto r = cache.get_or_load("k", 0, [&loads2](const std::string&) {
        ++loads2;
        return LoadResult::ok("y");
    });
    assert(loads2 == 1 && r.value == "y");
}

void test_loader_failure_shared_and_not_cached() {
    std::atomic<std::uint64_t> now{0};
    CoalescingCache cache(4, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    std::vector<LoadResult> results(6);
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < results.size(); ++i) {
        threads.emplace_back([&cache, &loads, &results, i] {
            results[i] = cache.get_or_load(
                "fail", 10, [&loads](const std::string&) {
                    ++loads;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    return LoadResult::failed("nope");
                });
        });
    }
    for (auto& t : threads) t.join();
    assert(loads == 1);
    assert(cache.size() == 0);  // failure not cached
    for (const auto& r : results) {
        assert(r.status == LoadStatus::loader_failed);
        assert(r.error == "nope");
    }
    // Subsequent call can reload.
    int loads2 = 0;
    auto r = cache.get_or_load("fail", 10, [&loads2](const std::string&) {
        ++loads2;
        return LoadResult::ok("recovered");
    });
    assert(loads2 == 1 && r.value == "recovered");
}

void test_loader_exception_caught() {
    std::atomic<std::uint64_t> now{0};
    CoalescingCache cache(4, [&now] { return now.load(); });
    auto r = cache.get_or_load("k", 10, [](const std::string&) -> LoadResult {
        throw std::runtime_error("boom");
    });
    assert(r.status == LoadStatus::loader_failed);
    assert(cache.size() == 0);
}

void test_recursive_same_key_returns_recursive() {
    std::atomic<std::uint64_t> now{0};
    CoalescingCache cache(4, [&now] { return now.load(); });
    int inner_loads = 0;
    auto r = cache.get_or_load("k", 10, [&](const std::string& key) {
        // Re-enter for the SAME key while owning the loader: must not wait.
        auto inner = cache.get_or_load(key, 10, [&inner_loads](const std::string&) {
            ++inner_loads;
            return LoadResult::ok("should-not-run");
        });
        assert(inner.status == LoadStatus::recursive_load);
        return LoadResult::ok("outer");
    });
    assert(r.status == LoadStatus::ok && r.value == "outer");
    assert(inner_loads == 0);
}

void test_recursive_different_key_works() {
    std::atomic<std::uint64_t> now{0};
    CoalescingCache cache(4, [&now] { return now.load(); });
    auto r = cache.get_or_load("outer", 10, [&](const std::string&) {
        // Different key from within a loader: must succeed, no deadlock.
        auto inner = cache.get_or_load("inner", 10, [](const std::string&) {
            return LoadResult::ok("inner-val");
        });
        assert(inner.status == LoadStatus::ok && inner.value == "inner-val");
        return LoadResult::ok("outer-val");
    });
    assert(r.status == LoadStatus::ok && r.value == "outer-val");
}

void test_invalidate_during_flight_skips_writeback() {
    std::atomic<std::uint64_t> now{0};
    CoalescingCache cache(4, [&now] { return now.load(); });
    std::atomic<bool> started{false};
    std::atomic<bool> invalidated{false};

    std::thread owner([&] {
        cache.get_or_load("k", 100, [&](const std::string&) {
            started.store(true);
            while (!invalidated.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return LoadResult::ok("v");
        });
    });

    // Wait until the owner is mid-load, then invalidate.
    while (!started.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    cache.invalidate("k");
    invalidated.store(true);
    owner.join();

    // Owner returned ok to its waiters but must NOT have cached (invalidate won).
    assert(cache.size() == 0);
}

void test_lru_eviction() {
    std::atomic<std::uint64_t> now{1};
    CoalescingCache cache(2, [&now] { return now.load(); });
    auto load = [](const std::string& key) { return LoadResult::ok(key); };

    // Fill cache: MRU order becomes [b, a] (a is LRU).
    assert(cache.get_or_load("a", 100, load).value == "a");
    assert(cache.get_or_load("b", 100, load).value == "b");
    assert(cache.size() == 2);

    // Access "a" to promote it to MRU: order now [a, b] (b is LRU).
    now = 2;
    int a_touch = 0;
    cache.get_or_load("a", 100, [&a_touch](const std::string&) {
        ++a_touch;
        return LoadResult::ok("a");
    });
    assert(a_touch == 0);  // a was cached, no reload

    // Insert "c": LRU ("b") is evicted. Order becomes [c, a].
    assert(cache.get_or_load("c", 100, load).value == "c");
    assert(cache.size() == 2);

    // a still cached; b evicted.
    int a_loads = 0, b_loads = 0;
    now = 3;
    cache.get_or_load("a", 100, [&a_loads](const std::string&) {
        ++a_loads;
        return LoadResult::ok("a");
    });
    cache.get_or_load("b", 100, [&b_loads](const std::string&) {
        ++b_loads;
        return LoadResult::ok("b");
    });
    assert(a_loads == 0);  // a survived
    assert(b_loads == 1);  // b was evicted by c
}

void test_capacity_zero_never_caches() {
    std::atomic<std::uint64_t> now{0};
    CoalescingCache cache(0, [&now] { return now.load(); });
    int loads = 0;
    auto r1 = cache.get_or_load("k", 100, [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("v");
    });
    assert(r1.status == LoadStatus::ok && cache.size() == 0);
    auto r2 = cache.get_or_load("k", 100, [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("v");
    });
    assert(loads == 2 && cache.size() == 0);
}

void test_invalidate_removes_ready() {
    std::atomic<std::uint64_t> now{0};
    CoalescingCache cache(4, [&now] { return now.load(); });
    int loads = 0;
    auto loader = [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("v");
    };
    now = 1;
    cache.get_or_load("k", 100, loader);
    assert(loads == 1 && cache.size() == 1);
    cache.invalidate("k");
    assert(cache.size() == 0);
    now = 2;
    cache.get_or_load("k", 100, loader);
    assert(loads == 2 && cache.size() == 1);
}

void test_stress_no_duplicate_loads() {
    std::atomic<std::uint64_t> now{1000};
    CoalescingCache cache(8, [&now] { return now.load(); });
    constexpr int N = 32;
    std::atomic<int> loads{0};
    std::vector<LoadResult> results(N);
    std::vector<std::thread> threads;
    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&cache, &loads, &results, i] {
            results[i] = cache.get_or_load(
                "hot", 1000, [&loads](const std::string&) {
                    ++loads;
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    return LoadResult::ok("data");
                });
        });
    }
    for (auto& t : threads) t.join();
    assert(loads.load() == 1);
    for (const auto& r : results) {
        assert(r.status == LoadStatus::ok && r.value == "data");
    }
    assert(cache.size() == 1);
}

}  // namespace

int main() {
    test_ttl_expiry_reload();
    test_ttl_zero_never_caches_but_coalesces();
    test_loader_failure_shared_and_not_cached();
    test_loader_exception_caught();
    test_recursive_same_key_returns_recursive();
    test_recursive_different_key_works();
    test_invalidate_during_flight_skips_writeback();
    test_lru_eviction();
    test_capacity_zero_never_caches();
    test_invalidate_removes_ready();
    test_stress_no_duplicate_loads();
    return 0;
}
