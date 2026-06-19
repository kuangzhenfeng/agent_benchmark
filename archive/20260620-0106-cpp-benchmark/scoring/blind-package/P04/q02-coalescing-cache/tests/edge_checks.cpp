#include "coalescing_cache.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

static void test_ttl_expiry_triggers_reload() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(4, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    auto loader = [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("v1");
    };
    auto r1 = cache.get_or_load("k", 10, loader);
    assert(r1.status == LoadStatus::ok && r1.value == "v1");
    assert(loads.load() == 1);
    assert(cache.size() == 1);

    auto r2 = cache.get_or_load("k", 10, loader);
    assert(r2.status == LoadStatus::ok && r2.value == "v1");
    assert(loads.load() == 1);

    now.store(110);
    auto r3 = cache.get_or_load("k", 10, loader);
    assert(r3.status == LoadStatus::ok);
    assert(loads.load() == 2);
}

static void test_ttl_zero_no_cache_but_coalesces() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(4, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    std::vector<LoadResult> results(4);
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < results.size(); ++i) {
        threads.emplace_back([&cache, &loads, &results, i] {
            results[i] = cache.get_or_load("k", 0, [&loads](const std::string&) {
                ++loads;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                return LoadResult::ok("x");
            });
        });
    }
    for (auto& t : threads) t.join();
    assert(loads.load() == 1);
    for (const auto& r : results) {
        assert(r.status == LoadStatus::ok && r.value == "x");
    }
    assert(cache.size() == 0);

    auto r2 = cache.get_or_load("k", 0, [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("y");
    });
    assert(loads.load() == 2);
}

static void test_loader_failure_not_cached_allows_reload() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(4, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    auto failing_loader = [&loads](const std::string&) {
        ++loads;
        return LoadResult::failed("nope");
    };
    auto r1 = cache.get_or_load("k", 10, failing_loader);
    assert(r1.status == LoadStatus::loader_failed);
    assert(r1.error == "nope");
    assert(cache.size() == 0);
    assert(loads.load() == 1);

    auto ok_loader = [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("v");
    };
    auto r2 = cache.get_or_load("k", 10, ok_loader);
    assert(r2.status == LoadStatus::ok && r2.value == "v");
    assert(loads.load() == 2);
}

static void test_loader_exception_caught_no_cross_boundary() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(4, [&now] { return now.load(); });
    auto throwing_loader = [](const std::string&) -> LoadResult {
        throw std::runtime_error("boom");
    };
    auto r = cache.get_or_load("k", 10, throwing_loader);
    assert(r.status == LoadStatus::loader_failed);
    assert(cache.size() == 0);
}

static void test_concurrent_failure_shared() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(4, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    std::vector<LoadResult> results(5);
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < results.size(); ++i) {
        threads.emplace_back([&cache, &loads, &results, i] {
            results[i] = cache.get_or_load("k", 10, [&loads](const std::string&) {
                ++loads;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                return LoadResult::failed("shared-fail");
            });
        });
    }
    for (auto& t : threads) t.join();
    assert(loads.load() == 1);
    for (const auto& r : results) {
        assert(r.status == LoadStatus::loader_failed);
        assert(r.error == "shared-fail");
    }
    assert(cache.size() == 0);
}

static void test_lru_eviction() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(2, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    auto loader = [&loads](const std::string& key) {
        ++loads;
        return LoadResult::ok(key + "-val");
    };
    cache.get_or_load("a", 100, loader);
    cache.get_or_load("b", 100, loader);
    assert(cache.size() == 2);
    assert(loads.load() == 2);

    cache.get_or_load("a", 100, loader);
    assert(loads.load() == 2);

    cache.get_or_load("c", 100, loader);
    assert(cache.size() == 2);
    assert(loads.load() == 3);

    now.store(101);
    auto rb = cache.get_or_load("b", 100, loader);
    assert(rb.status == LoadStatus::ok);
    assert(loads.load() == 4);
}

static void test_capacity_zero_never_caches() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(0, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    auto loader = [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("v");
    };
    auto r = cache.get_or_load("k", 10, loader);
    assert(r.status == LoadStatus::ok && r.value == "v");
    assert(cache.size() == 0);
    assert(loads.load() == 1);

    auto r2 = cache.get_or_load("k", 10, loader);
    assert(r2.status == LoadStatus::ok);
    assert(loads.load() == 2);
}

static void test_recursive_same_key() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(4, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    auto loader = [&cache, &loads](const std::string& key) -> LoadResult {
        ++loads;
        auto inner = cache.get_or_load(key, 10, [](const std::string&) {
            return LoadResult::ok("never");
        });
        assert(inner.status == LoadStatus::recursive_load);
        return LoadResult::ok("outer");
    };
    auto r = cache.get_or_load("k", 10, loader);
    assert(r.status == LoadStatus::ok && r.value == "outer");
    assert(loads.load() == 1);
}

static void test_reentrant_different_key_no_deadlock() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(4, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    auto loader_a = [&cache, &loads](const std::string&) -> LoadResult {
        ++loads;
        auto inner = cache.get_or_load("b", 10, [&loads](const std::string&) {
            ++loads;
            return LoadResult::ok("b-val");
        });
        assert(inner.status == LoadStatus::ok && inner.value == "b-val");
        return LoadResult::ok("a-val");
    };
    auto r = cache.get_or_load("a", 10, loader_a);
    assert(r.status == LoadStatus::ok && r.value == "a-val");
    assert(loads.load() == 2);
}

static void test_invalidate_removes_ready() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(4, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    auto loader = [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("v");
    };
    cache.get_or_load("k", 100, loader);
    assert(cache.size() == 1);
    assert(loads.load() == 1);

    cache.invalidate("k");
    assert(cache.size() == 0);

    auto r = cache.get_or_load("k", 100, loader);
    assert(r.status == LoadStatus::ok);
    assert(loads.load() == 2);
}

static void test_invalidate_during_load_prevents_writeback() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(4, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    std::atomic<bool> loader_started{false};

    std::thread loader_thread([&] {
        auto r = cache.get_or_load("k", 100, [&loads, &loader_started](const std::string&) {
            ++loads;
            loader_started.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return LoadResult::ok("v");
        });
        assert(r.status == LoadStatus::ok && r.value == "v");
    });

    while (!loader_started.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    cache.invalidate("k");
    loader_thread.join();

    assert(loads.load() == 1);
    assert(cache.size() == 0);

    auto r2 = cache.get_or_load("k", 100, [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("v2");
    });
    assert(r2.status == LoadStatus::ok && r2.value == "v2");
    assert(loads.load() == 2);
}

static void test_invalidate_during_load_joiners_share_result() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(4, [&now] { return now.load(); });
    std::atomic<bool> loader_started{false};
    std::atomic<int> loads{0};

    std::thread joiner([&] {
        while (!loader_started.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        auto r = cache.get_or_load("k", 100, [](const std::string&) {
            return LoadResult::ok("wrong");
        });
        assert(r.status == LoadStatus::ok && r.value == "v");
    });

    auto r0 = cache.get_or_load("k", 100, [&loads, &loader_started](const std::string&) {
        ++loads;
        loader_started.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        return LoadResult::ok("v");
    });
    assert(r0.status == LoadStatus::ok && r0.value == "v");

    std::thread invoker([&] {
        while (!loader_started.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        cache.invalidate("k");
    });

    joiner.join();
    invoker.join();
    assert(loads.load() == 1);
    assert(cache.size() == 0);
}

static void test_lru_access_refreshes_position() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(2, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    auto loader = [&loads](const std::string& key) {
        ++loads;
        return LoadResult::ok(key);
    };
    cache.get_or_load("a", 100, loader);
    cache.get_or_load("b", 100, loader);

    now.store(101);
    cache.get_or_load("a", 100, loader);
    assert(loads.load() == 2);

    cache.get_or_load("c", 100, loader);
    assert(loads.load() == 3);

    auto rb = cache.get_or_load("b", 100, loader);
    assert(loads.load() == 4);
    assert(rb.value == "b");
}

static void test_concurrent_distinct_keys() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(100, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    const int N = 50;
    std::vector<std::thread> threads;
    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&cache, &loads, i] {
            std::string key = "k" + std::to_string(i % 10);
            auto r = cache.get_or_load(key, 1000, [&loads](const std::string& k) {
                ++loads;
                return LoadResult::ok(k + "-v");
            });
            assert(r.status == LoadStatus::ok);
        });
    }
    for (auto& t : threads) t.join();
    assert(loads.load() == 10);
    assert(cache.size() == 10);
}

int main() {
    test_ttl_expiry_triggers_reload();
    test_ttl_zero_no_cache_but_coalesces();
    test_loader_failure_not_cached_allows_reload();
    test_loader_exception_caught_no_cross_boundary();
    test_concurrent_failure_shared();
    test_lru_eviction();
    test_capacity_zero_never_caches();
    test_recursive_same_key();
    test_reentrant_different_key_no_deadlock();
    test_invalidate_removes_ready();
    test_invalidate_during_load_prevents_writeback();
    test_invalidate_during_load_joiners_share_result();
    test_lru_access_refreshes_position();
    test_concurrent_distinct_keys();
    return 0;
}
