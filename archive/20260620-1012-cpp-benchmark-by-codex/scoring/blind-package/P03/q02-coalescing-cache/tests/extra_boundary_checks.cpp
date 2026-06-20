// Extra boundary checks for CoalescingCache.
#include "coalescing_cache.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            ++failures;                                                   \
            std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond); \
        }                                                                 \
    } while (0)

// 1. Loader runs at most once for concurrent miss; all get same value.
static void test_coalesce_basic() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(2, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    std::vector<LoadResult> results(8);
    std::vector<std::thread> ts;
    for (std::size_t i = 0; i < results.size(); ++i) {
        ts.emplace_back([&, i] {
            results[i] = cache.get_or_load("k", 10, [&](const std::string&) {
                ++loads;
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                return LoadResult::ok("v");
            });
        });
    }
    for (auto& t : ts) t.join();
    CHECK(loads.load() == 1);
    for (auto& r : results) { CHECK(r.status == LoadStatus::ok); CHECK(r.value == "v"); }
    CHECK(cache.size() == 1);
}

// 2. TTL expiry triggers reload; TTL anchor = loader completion clock.
static void test_ttl_expiry() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(4, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    auto loader = [&](const std::string&) {
        int n = ++loads;
        return LoadResult::ok("v" + std::to_string(n));
    };
    auto r1 = cache.get_or_load("k", 10, loader);  // loaded_at=100
    CHECK(r1.value == "v1");
    now.store(105);                                 // 105-100=5 < 10
    auto r2 = cache.get_or_load("k", 10, loader);
    CHECK(r2.value == "v1");                        // cached
    now.store(110);                                 // 110-100=10 >= 10 expired
    auto r3 = cache.get_or_load("k", 10, loader);
    CHECK(r3.value == "v2");                        // reloaded
    CHECK(loads.load() == 2);
}

// 3. TTL == 0: never cached, but concurrent callers still coalesce the one load.
static void test_ttl_zero_coalesce() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(2, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    std::vector<LoadResult> results(6);
    std::vector<std::thread> ts;
    for (std::size_t i = 0; i < results.size(); ++i) {
        ts.emplace_back([&, i] {
            results[i] = cache.get_or_load("k", 0, [&](const std::string&) {
                ++loads;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                return LoadResult::ok("v");
            });
        });
    }
    for (auto& t : ts) t.join();
    CHECK(loads.load() == 1);          // coalesced into one load
    CHECK(cache.size() == 0);          // but nothing cached
    for (auto& r : results) { CHECK(r.status == LoadStatus::ok); CHECK(r.value == "v"); }
}

// 4. Loader failure: no ready cache; waiters all get failure; can reload.
static void test_loader_failure() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(2, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    auto loader = [&](const std::string&) -> LoadResult {
        int n = ++loads;
        if (n == 1) return LoadResult::failed("boom");
        return LoadResult::ok("v" + std::to_string(n));
    };
    auto r1 = cache.get_or_load("k", 10, loader);
    CHECK(r1.status == LoadStatus::loader_failed);
    CHECK(cache.size() == 0);
    auto r2 = cache.get_or_load("k", 10, loader);  // reload allowed
    CHECK(r2.status == LoadStatus::ok);
    CHECK(r2.value == "v2");
}

// 5. Loader throws: exception does not cross API boundary; waiters get failure.
static void test_loader_throws() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(2, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    auto loader = [&](const std::string&) -> LoadResult {
        ++loads;
        throw std::runtime_error("ouch");
    };
    bool caught = false;
    try {
        cache.get_or_load("k", 10, loader);
    } catch (...) {
        caught = true;
    }
    CHECK(!caught);
    CHECK(loads.load() == 1);
    CHECK(cache.size() == 0);
}

// 6. Recursive same-key load by owner returns recursive_load immediately.
static void test_recursive_load() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(2, [&now] { return now.load(); });
    std::atomic<int> inner_status{-1};
    auto loader = [&](const std::string& key) -> LoadResult {
        auto inner = cache.get_or_load(key, 10, [](const std::string&) {
            return LoadResult::ok("never");
        });
        inner_status.store(static_cast<int>(inner.status));
        return LoadResult::ok("outer");
    };
    auto r = cache.get_or_load("k", 10, loader);
    CHECK(r.status == LoadStatus::ok);
    CHECK(inner_status.load() == static_cast<int>(LoadStatus::recursive_load));
}

// 7. invalidate removes ready; concurrent in-flight not cancelled; not re-cached.
static void test_invalidate_race() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(2, [&now] { return now.load(); });
    std::atomic<bool> loader_started{false};
    std::atomic<bool> invalidate_done{false};

    auto loader = [&](const std::string&) -> LoadResult {
        loader_started.store(true);
        // wait until the main thread invalidates mid-load
        while (!invalidate_done.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return LoadResult::ok("v");
    };

    LoadResult outer;
    std::thread owner([&] { outer = cache.get_or_load("k", 10, loader); });
    while (!loader_started.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    // invalidate while in-flight: ready removed (none yet) + flag flight.
    cache.invalidate("k");
    CHECK(cache.size() == 0);
    invalidate_done.store(true);
    owner.join();
    CHECK(outer.status == LoadStatus::ok);   // owner still gets the value
    CHECK(cache.size() == 0);                // but NOT written back to ready cache
    // a later call reloads fresh:
    std::atomic<int> loads2{0};
    auto r2 = cache.get_or_load("k", 10, [&](const std::string&) {
        ++loads2; return LoadResult::ok("w");
    });
    CHECK(r2.value == "w");
    CHECK(loads2.load() == 1);
}

// 8. LRU eviction at capacity; access refreshes position; in-flight not evicted.
static void test_lru() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(2, [&now] { return now.load(); });
    auto loader = [](const std::string& key) { return LoadResult::ok(key); };
    cache.get_or_load("a", 100, loader);
    cache.get_or_load("b", 100, loader);
    CHECK(cache.size() == 2);
    // touch "a" -> a becomes MRU, b becomes LRU
    cache.get_or_load("a", 100, loader);
    // insert "c": evicts LRU (b)
    cache.get_or_load("c", 100, loader);
    CHECK(cache.size() == 2);
    now.store(100);
    auto rb = cache.get_or_load("b", 100, loader);  // must reload (evicted)
    CHECK(rb.value == "b");
}

// 9. Capacity 0: never caches, but still coalesces.
static void test_capacity_zero() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(0, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    auto loader = [&](const std::string&) {
        ++loads; std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return LoadResult::ok("v");
    };
    std::vector<std::thread> ts;
    for (int i = 0; i < 4; ++i) ts.emplace_back([&]{ cache.get_or_load("k", 50, loader); });
    for (auto& t : ts) t.join();
    CHECK(loads.load() == 1);
    CHECK(cache.size() == 0);
}

// 10. size() self-contained, reflects ready cache only.
static void test_size_isolated() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(3, [&now] { return now.load(); });
    auto loader = [](const std::string& k) { return LoadResult::ok(k); };
    cache.get_or_load("x", 50, loader);
    CHECK(cache.size() == 1);
    CHECK(cache.size() == 1);  // repeatable
}

// 11. Heavy concurrency stress (different keys + occasional same key) no deadlock.
static void test_stress() {
    std::atomic<std::uint64_t> now{1000};
    CoalescingCache cache(8, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    std::vector<std::thread> ts;
    std::atomic<bool> stop{false};
    for (int i = 0; i < 6; ++i) {
        ts.emplace_back([&, i] {
            int n = 0;
            while (!stop.load()) {
                std::string key = "k" + std::to_string((i + n) % 5);
                cache.get_or_load(key, 5, [&](const std::string&) {
                    ++loads;
                    return LoadResult::ok(key);
                });
                if ((n % 3) == 0) cache.invalidate("k" + std::to_string(n % 5));
                ++n;
            }
        });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    stop.store(true);
    for (auto& t : ts) t.join();
    CHECK(loads.load() > 0);
    CHECK(cache.size() <= 8);
}

int main() {
    test_coalesce_basic();
    test_ttl_expiry();
    test_ttl_zero_coalesce();
    test_loader_failure();
    test_loader_throws();
    test_recursive_load();
    test_invalidate_race();
    test_lru();
    test_capacity_zero();
    test_size_isolated();
    test_stress();
    if (failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURES\n", failures);
    return 1;
}
