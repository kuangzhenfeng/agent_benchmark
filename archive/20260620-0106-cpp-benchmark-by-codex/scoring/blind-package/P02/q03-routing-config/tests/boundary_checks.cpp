// 边界语义自测：先校验后发布、版本严格加一、snapshot 完整自洽、
// current() 引用保活、观察者重入与异常隔离、最长前缀匹配、并发。
#include "routing_config.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
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

// 语义1/2：校验失败不变；成功版本严格加一；snapshot 完整自洽。
static void test_validate_and_version() {
    RoutingConfig config(Config{0, {{"/", "default"}}});
    CHECK(config.current().version == 0);

    CHECK(!config.reload(Config{0, {{"/", ""}}}));               // 空 target
    CHECK(!config.reload(Config{0, {{"", "t"}}}));               // 空 prefix
    CHECK(!config.reload(Config{0, {{"/", "a"}, {"/", "b"}}}));  // 重复 prefix

    CHECK(config.current().version == 0);  // 失败不变
    CHECK(config.snapshot()->version == 0);

    CHECK(config.reload(Config{0, {{"/", "default"}, {"/v1", "blue"}}}));
    CHECK(config.current().version == 1);  // 严格加一
    CHECK(config.snapshot()->version == 1);

    CHECK(config.reload(Config{0, {{"/", "default"}, {"/v1", "green"}, {"/v1/users", "svc"}}}));
    CHECK(config.current().version == 2);
}

// 语义3：current() 返回的引用在该线程下次调用 current() 前及后续 reload 中有效。
static void test_current_reference_lifetime() {
    RoutingConfig config(Config{0, {{"/", "default"}}});
    const Config* p1 = &config.current();
    // 在不再次调用 current() 的情况下连续 reload：p1 仍应有效。
    for (int i = 0; i < 50; ++i) {
        config.reload(Config{0, {{"/", "v" + std::to_string(i)}}});
    }
    CHECK(p1->version == 0);              // 指向最初快照，未被释放
    CHECK(p1->routes[0].target == "default");

    const Config* p2 = &config.current(); // 切换 pin 到最新
    CHECK(p2 != p1);
    CHECK(p2->version == 50);
}

// 语义6：最长前缀匹配；无匹配返回空。
static void test_find_target_longest_prefix() {
    RoutingConfig config(Config{0, {{"/", "root"}, {"/v1", "v1"}, {"/v1/users", "users"}}});
    CHECK(config.find_target("/v1/users/42") == "users");
    CHECK(config.find_target("/v1/groups") == "v1");
    CHECK(config.find_target("/health") == "root");
    CHECK(config.find_target("nomatch") == "");  // 不以任何 prefix 开头

    RoutingConfig c2(Config{0, {{"/api", "api"}}});
    CHECK(c2.find_target("/api/x") == "api");
    CHECK(c2.find_target("/other") == "");
}

// 语义4：观察者重入 subscribe/unsubscribe/reload；回调中取消只影响后续 reload；
//        回调中新建订阅不接收当前通知；每个观察者至多通知一次。
static void test_observer_reentrancy() {
    RoutingConfig config(Config{0, {{"/", "default"}}});
    std::atomic<int> a_count{0}, b_count{0};
    RoutingConfig::ObserverId a_id = 0;
    bool first = true;

    a_id = config.subscribe([&](std::shared_ptr<const Config>) {
        ++a_count;
        if (first) {
            first = false;
            config.unsubscribe(a_id);   // 回调中取消自己：只影响后续 reload
            config.subscribe([&](std::shared_ptr<const Config>) {
                ++b_count;              // 新订阅：不接收当前通知
            });
        }
    });

    CHECK(config.reload(Config{0, {{"/", "r1"}}}));
    CHECK(a_count.load() == 1);   // a 收到当前通知
    CHECK(b_count.load() == 0);   // b 新建，不收当前

    CHECK(config.reload(Config{0, {{"/", "r2"}}}));
    CHECK(a_count.load() == 1);   // a 已取消
    CHECK(b_count.load() == 1);   // b 收后续 reload
    (void)a_id;

    // 回调中重入 reload + snapshot 不死锁。
    // 用独立实例隔离：观察者对 inner 做 reload，inner 的通知不会递归回本观察者，
    // 避免观察者自我通知导致的（逻辑层）无限递归。
    RoutingConfig inner(Config{0, {{"/", "default"}}});
    std::atomic<bool> reloaded_inside{false};
    config.subscribe([&](std::shared_ptr<const Config>) {
        std::uint64_t before = inner.snapshot()->version;
        if (inner.reload(Config{0, {{"/", "inside"}}})) {
            reloaded_inside.store(inner.snapshot()->version == before + 1);
        }
    });
    CHECK(config.reload(Config{0, {{"/", "trigger"}}}));
    CHECK(reloaded_inside.load());
}

// 语义5：观察者抛异常不阻止其余观察者，也不回滚已发布配置；计数累加。
static void test_observer_exception_isolation() {
    RoutingConfig config(Config{0, {{"/", "default"}}});
    std::atomic<int> good_count{0};
    config.subscribe([](std::shared_ptr<const Config>) {
        throw std::runtime_error("boom");
    });
    config.subscribe([&good_count](std::shared_ptr<const Config>) {
        ++good_count;
    });
    config.subscribe([](std::shared_ptr<const Config>) {
        throw std::logic_error("boom2");
    });

    CHECK(config.reload(Config{0, {{"/", "new"}}}));
    CHECK(good_count.load() == 1);              // 中间观察者仍执行
    CHECK(config.current().version == 1);       // 已发布配置不回滚
    CHECK(config.observer_error_count() == 2);  // 两次异常累加

    CHECK(config.reload(Config{0, {{"/", "new2"}}}));
    CHECK(config.observer_error_count() == 4);  // 继续累加
}

// 语义2/7：并发 reader 看到一致快照；并发 reload 版本严格递增到目标值。
static void test_concurrent() {
    RoutingConfig config(Config{0, {{"/", "default"}}});
    std::atomic<bool> stop{false};

    std::vector<std::thread> ts;
    // readers：反复 snapshot/current/find_target，不应崩溃或读到悬空。
    for (int i = 0; i < 4; ++i) {
        ts.emplace_back([&] {
            while (!stop.load()) {
                auto snap = config.snapshot();
                (void)snap->version;
                (void)config.current().version;
                (void)config.find_target("/x");
            }
        });
    }
    // writers
    for (int i = 0; i < 4; ++i) {
        ts.emplace_back([&] {
            for (int j = 0; j < 200; ++j) {
                config.reload(Config{0, {{"/", "w" + std::to_string(j)}}});
            }
        });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop.store(true);
    for (auto& t : ts) t.join();

    // 最终版本应等于成功 reload 次数（4*200=800），严格加一。
    CHECK(config.current().version == 800);
}

int main() {
    test_validate_and_version();
    test_current_reference_lifetime();
    test_find_target_longest_prefix();
    test_observer_reentrancy();
    test_observer_exception_isolation();
    test_concurrent();
    if (failures == 0) {
        std::cout << "ALL BOUNDARY TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
