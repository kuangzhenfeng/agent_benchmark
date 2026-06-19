// 覆盖并发与失效边界的自测。
#include "coalescing_cache.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond << "\n";  \
            ++failures;                                                   \
        }                                                                 \
    } while (0)

// 简易屏障：让 N 个线程同时放开。
struct Barrier {
    std::mutex m;
    std::condition_variable cv;
    int remain;
    explicit Barrier(int n) : remain(n) {}
    void wait() {
        std::unique_lock<std::mutex> lk(m);
        if (--remain == 0) {
            cv.notify_all();
        } else {
            cv.wait(lk, [this] { return remain == 0; });
        }
    }
};

// 语义1：并发 miss 合并，至多一个 loader。
static void test_concurrent_merge() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(2, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    std::vector<LoadResult> results(6);
    Barrier bar((int)results.size());
    std::vector<std::thread> ts;
    for (std::size_t i = 0; i < results.size(); ++i) {
        ts.emplace_back([&, i] {
            bar.wait();
            results[i] = cache.get_or_load("profile:7", 10, [&](const std::string&) {
                ++loads;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                return LoadResult::ok("alice");
            });
        });
    }
    for (auto& t : ts) t.join();
    CHECK(loads.load() == 1);
    for (auto& r : results) {
        CHECK(r.status == LoadStatus::ok);
        CHECK(r.value == "alice");
    }
    CHECK(cache.size() == 1);
}

// 语义4：loader 失败时，等待者都得到失败；失败不进 ready；之后可重新加载。
static void test_failure_shared() {
    std::atomic<std::uint64_t> now{1};
    CoalescingCache cache(2, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    std::vector<LoadResult> results(4);
    Barrier bar((int)results.size());
    std::vector<std::thread> ts;
    for (std::size_t i = 0; i < results.size(); ++i) {
        ts.emplace_back([&, i] {
            bar.wait();
            results[i] = cache.get_or_load("k", 10, [&](const std::string&) {
                ++loads;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                return LoadResult::failed("nope");
            });
        });
    }
    for (auto& t : ts) t.join();
    CHECK(loads.load() == 1);
    for (auto& r : results) {
        CHECK(r.status == LoadStatus::loader_failed);
        CHECK(r.error == "nope");
    }
    CHECK(cache.size() == 0);  // 失败不进 ready
    // 之后可重新加载。
    auto r2 = cache.get_or_load("k", 10, [](const std::string&) {
        return LoadResult::ok("ok2");
    });
    CHECK(r2.status == LoadStatus::ok);
    CHECK(r2.value == "ok2");
}

// 语义4：loader 抛异常不跨边界，等待者得到失败。
static void test_loader_throws() {
    std::atomic<std::uint64_t> now{1};
    CoalescingCache cache(2, [&now] { return now.load(); });
    std::vector<LoadResult> results(3);
    Barrier bar((int)results.size());
    std::vector<std::thread> ts;
    for (std::size_t i = 0; i < results.size(); ++i) {
        ts.emplace_back([&, i] {
            bar.wait();
            results[i] = cache.get_or_load("k", 10, [](const std::string&) -> LoadResult {
                throw std::runtime_error("boom");
            });
        });
    }
    for (auto& t : ts) t.join();
    for (auto& r : results) {
        CHECK(r.status == LoadStatus::loader_failed);
    }
    CHECK(cache.size() == 0);
}

// 语义2：重入不同 key 不死锁；重入同 key 返回 recursive_load。
static void test_reentrancy() {
    std::atomic<std::uint64_t> now{1};
    CoalescingCache cache(4, [&now] { return now.load(); });

    LoadResult inner_diff;
    LoadResult inner_same;
    cache.get_or_load("outer", 10, [&](const std::string&) {
        // 重入不同 key：不得死锁，正常返回。
        inner_diff = cache.get_or_load("inner", 10, [](const std::string&) {
            return LoadResult::ok("inner-val");
        });
        // 重入同一 key：立即返回 recursive_load，不等待自己。
        inner_same = cache.get_or_load("outer", 10, [](const std::string&) {
            return LoadResult::ok("never");
        });
        return LoadResult::ok("outer-val");
    });
    CHECK(inner_diff.status == LoadStatus::ok);
    CHECK(inner_diff.value == "inner-val");
    CHECK(inner_same.status == LoadStatus::recursive_load);
}

// 语义3：TTL 到期重新加载；TTL=0 不缓存但仍合并同一次在途。
static void test_ttl() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(2, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    auto loader = [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("v");
    };
    auto r1 = cache.get_or_load("k", 10, loader);
    CHECK(loads.load() == 1);
    now = 105;  // 未到期
    auto r2 = cache.get_or_load("k", 10, loader);
    CHECK(loads.load() == 1);  // 命中缓存
    now = 110;  // 刚到 expire_time(110)，< 不成立，重新加载
    auto r3 = cache.get_or_load("k", 10, loader);
    CHECK(loads.load() == 2);  // 到期重载

    // TTL=0 不缓存。
    std::atomic<int> loads2{0};
    auto r4 = cache.get_or_load("z", 0, [&loads2](const std::string&) {
        ++loads2;
        return LoadResult::ok("zv");
    });
    CHECK(r4.status == LoadStatus::ok);
    CHECK(loads2.load() == 1);
    auto r5 = cache.get_or_load("z", 0, [&loads2](const std::string&) {
        ++loads2;
        return LoadResult::ok("zv");
    });
    CHECK(loads2.load() == 2);  // 未缓存，每次加载
}

// 语义3/1：TTL=0 时并发调用仍合并同一次在途加载。
static void test_ttl0_concurrent_merges() {
    std::atomic<std::uint64_t> now{1};
    CoalescingCache cache(2, [&now] { return now.load(); });
    std::atomic<int> loads{0};
    std::vector<LoadResult> results(4);
    Barrier bar((int)results.size());
    std::vector<std::thread> ts;
    for (std::size_t i = 0; i < results.size(); ++i) {
        ts.emplace_back([&, i] {
            bar.wait();
            results[i] = cache.get_or_load("k", 0, [&](const std::string&) {
                ++loads;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                return LoadResult::ok("v");
            });
        });
    }
    for (auto& t : ts) t.join();
    CHECK(loads.load() == 1);  // 合并为一次在途
    for (auto& r : results) CHECK(r.status == LoadStatus::ok);
    CHECK(cache.size() == 0);  // TTL=0 不进 ready
}

// 语义5：invalidate 立即移除 ready；加载中不取消 loader 但不写回 ready；
//        之后调用可加入这次在途加载。
static void test_invalidate_race() {
    std::atomic<std::uint64_t> now{1};
    CoalescingCache cache(2, [&now] { return now.load(); });

    // 先填充 ready。
    cache.get_or_load("k", 100, [](const std::string&) { return LoadResult::ok("v1"); });
    CHECK(cache.size() == 1);
    cache.invalidate("k");
    CHECK(cache.size() == 0);  // 立即移除

    // 发起一个慢加载，期间 invalidate，验证 loader 不被取消但结果不写回 ready。
    std::atomic<bool> loader_started{false};
    std::atomic<bool> loader_can_finish{false};
    std::thread loader_thread([&] {
        cache.get_or_load("k", 100, [&](const std::string&) {
            loader_started = true;
            while (!loader_can_finish.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return LoadResult::ok("v2");
        });
    });
    // 等 loader 开始。
    while (!loader_started.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    cache.invalidate("k");  // 加载中失效：标记 discard
    loader_can_finish = true;
    loader_thread.join();
    CHECK(cache.size() == 0);  // 不写回 ready

    // 之后调用可重新加载（写入 ready）。
    auto r = cache.get_or_load("k", 100, [](const std::string&) {
        return LoadResult::ok("v3");
    });
    CHECK(r.status == LoadStatus::ok);
    CHECK(r.value == "v3");
    CHECK(cache.size() == 1);
}

// 语义6：LRU 淘汰；访问刷新 LRU；容量 0 永不保留。
static void test_lru() {
    std::atomic<std::uint64_t> now{1};
    std::atomic<int> loads{0};
    auto loader = [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("v");
    };
    CoalescingCache cache(2, [&now] { return now.load(); });
    cache.get_or_load("a", 100, loader);  // [a], loads=1
    cache.get_or_load("b", 100, loader);  // [b,a] (front=MRU), loads=2
    // 访问 a 刷新为 MRU：[a,b]（b 成为 LRU）
    cache.get_or_load("a", 100, loader);  // hit, loads=2
    // 插入 c 淘汰 LRU(b)：[c,a]，loads=3
    cache.get_or_load("c", 100, loader);  // miss, loads=3
    CHECK(cache.size() == 2);
    // a 仍命中（刷新后存活）
    cache.get_or_load("a", 100, loader);  // hit, loads=3
    CHECK(loads.load() == 3);
    // b 被淘汰（强制加载）
    cache.get_or_load("b", 100, loader);  // miss, loads=4；加载 b 时淘汰 LRU(c)
    CHECK(loads.load() == 4);
    // a 仍命中（此前被 touch 保护，未被这次加载淘汰）
    cache.get_or_load("a", 100, loader);  // hit, loads=4
    CHECK(loads.load() == 4);
    // c 已被淘汰（强制加载）
    cache.get_or_load("c", 100, loader);  // miss, loads=5
    CHECK(loads.load() == 5);

    // 容量 0：永不保留。
    CoalescingCache zc(0, [&now] { return now.load(); });
    auto rz = zc.get_or_load("x", 100, [](const std::string&) { return LoadResult::ok("X"); });
    CHECK(rz.status == LoadStatus::ok);
    CHECK(zc.size() == 0);
}

int main() {
    test_concurrent_merge();
    test_failure_shared();
    test_loader_throws();
    test_reentrancy();
    test_ttl();
    test_ttl0_concurrent_merges();
    test_invalidate_race();
    test_lru();
    if (failures == 0) {
        std::cout << "ALL BOUNDARY TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
