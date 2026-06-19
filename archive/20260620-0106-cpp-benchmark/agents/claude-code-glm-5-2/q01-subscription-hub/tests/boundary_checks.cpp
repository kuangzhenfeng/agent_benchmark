// 边界语义自测：临时字符串、回调重入、批次快照、取消/新增订阅、异常隔离、
// 32 位时钟回绕、多线程调用。不改变公开 API。
#include "subscription_hub.hpp"

#include <atomic>
#include <cassert>
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

// 语义1：临时 string 的 string_view 输入必须自洽存储。
static void test_temporary_string_ownership() {
    SubscriptionHub hub;
    std::vector<std::string> received;
    hub.subscribe(std::string("orders"), [&received](const SubscriptionHub::Event& e) {
        received.emplace_back(e.payload);
    }, 0);
    {
        std::string tmp_topic = "orders";
        std::string tmp_payload = "created";
        hub.publish(tmp_topic, tmp_payload);  // 临时字符串
    }  // tmp 析构
    auto stats = hub.drain(1);
    CHECK(stats.delivered == 1);
    CHECK((received == std::vector<std::string>{"created"}));
    CHECK(hub.pending_events() == 0);
}

// 语义2/3：回调中 publish 的事件必须留给下一次 drain。
static void test_reentrant_publish_deferred() {
    SubscriptionHub hub;
    std::vector<std::string> seen;
    bool reentered = false;
    hub.subscribe("topic", [&](const SubscriptionHub::Event& e) {
        seen.emplace_back(e.payload);
        if (!reentered) {
            reentered = true;
            hub.publish("topic", "second");  // 回调中发布，不得在本轮处理
        }
    }, 0);
    hub.publish("topic", "first");
    auto s1 = hub.drain(1);
    CHECK(s1.delivered == 1);
    CHECK((seen == std::vector<std::string>{"first"}));
    CHECK(hub.pending_events() == 1);  // second 仍在队列
    auto s2 = hub.drain(2);
    CHECK(s2.delivered == 1);
    CHECK((seen == (std::vector<std::string>{"first", "second"})));
}

// 语义3：回调中再 drain 不会无限递归处理当前批次——会处理新一轮（新发布的）。
//        关键是不能死锁/栈爆。这里回调中 drain 一个空队列确保不死锁。
static void test_reentrant_drain_no_deadlock() {
    SubscriptionHub hub;
    bool ok = false;
    hub.subscribe("t", [&](const SubscriptionHub::Event&) {
        hub.drain(1);  // 重入 drain，无事件
        ok = true;
    }, 0);
    hub.publish("t", "x");
    auto s = hub.drain(2);
    CHECK(s.delivered == 1);
    CHECK(ok);
}

// 语义4：回调中取消订阅阻止后续事件；回调中新建订阅不接收本次 drain 已截取事件。
static void test_unsubscribe_and_subscribe_in_callback() {
    SubscriptionHub hub;
    std::vector<std::string> a_seen;
    // 先用占位 id，subscribe 返回后赋值，使回调可捕获。
    SubscriptionHub::SubscriptionId id_a = 0;
    bool fired_new = false;
    id_a = hub.subscribe("t", [&](const SubscriptionHub::Event& e) {
        a_seen.emplace_back(e.payload);
        if (a_seen.size() == 1) {
            hub.unsubscribe(id_a);           // 取消自己：当前事件仍投递，后续不投递
            hub.subscribe("t", [&fired_new](const SubscriptionHub::Event&) {  // 新建订阅
                fired_new = true;            // 不应收到本次 drain 的事件
            }, 0);
        }
    }, 0);
    hub.publish("t", "e1");
    hub.publish("t", "e2");
    hub.drain(1);  // 处理两个事件
    CHECK((a_seen == std::vector<std::string>{"e1"}));  // e2 因取消被跳过
    CHECK(!fired_new);  // 新订阅本次 drain 未收到
}

// 语义5：单个回调抛异常不阻止其余回调；delivered 计数含异常；errors 单独计数；
//        该事件对已尝试订阅者视为处理完成（下次不重复）。
static void test_exception_isolation() {
    SubscriptionHub hub;
    std::vector<std::string> seen;
    hub.subscribe("t", [](const SubscriptionHub::Event&) {
        throw std::runtime_error("boom");
    }, 0);
    hub.subscribe("t", [&seen](const SubscriptionHub::Event& e) {
        seen.emplace_back(e.payload);
    }, 0);
    hub.publish("t", "payload");
    auto s = hub.drain(1);
    CHECK(s.delivered == 2);              // 两次尝试调用
    CHECK(s.callback_errors == 1);        // 一次异常
    CHECK((seen == std::vector<std::string>{"payload"}));  // 第二个回调仍执行
    CHECK(hub.pending_events() == 0);     // 事件不会被重新投递
    auto s2 = hub.drain(2);               // 再次 drain：无事件
    CHECK(s2.delivered == 0);
}

// 语义6：32 位模运算回绕。
static void test_32bit_wraparound() {
    SubscriptionHub hub;
    // last_active_tick = 0xFFFFFFFA（near max），now=5（已回绕）。
    // 真实经过时间 = 5 - 0xFFFFFFFA (mod 2^32) = 11 < 100，未过期。
    hub.subscribe("t", [](const SubscriptionHub::Event&) {}, 0xFFFFFFFAu);
    std::size_t expired = hub.expire_idle(5u, 100u);
    CHECK(expired == 0);

    // ttl=0 时立即过期（两个订阅都因 ttl==0 被移除）。
    hub.subscribe("t", [](const SubscriptionHub::Event&) {}, 5u);
    std::size_t exp2 = hub.expire_idle(5u, 0u);
    CHECK(exp2 == 2);
}

// 语义7：多线程 publish/subscribe/drain 并发不崩溃、不丢统计。
static void test_multithreaded() {
    SubscriptionHub hub;
    std::atomic<std::size_t> count{0};
    hub.subscribe("t", [&count](const SubscriptionHub::Event&) { ++count; }, 0);
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&hub, i] {
            for (int j = 0; j < 100; ++j) {
                hub.publish("t", std::to_string(i * 1000 + j));
            }
        });
    }
    std::thread drainer([&hub] {
        for (int k = 0; k < 50; ++k) {
            hub.drain(1);
        }
    });
    for (auto& th : threads) th.join();
    // 排空剩余
    hub.drain(2);
    drainer.join();
    std::size_t total_published = 8 * 100;
    CHECK(count.load() == total_published);
    CHECK(hub.pending_events() == 0);
}

int main() {
    test_temporary_string_ownership();
    test_reentrant_publish_deferred();
    test_reentrant_drain_no_deadlock();
    test_unsubscribe_and_subscribe_in_callback();
    test_exception_isolation();
    test_32bit_wraparound();
    test_multithreaded();
    if (failures == 0) {
        std::cout << "ALL BOUNDARY TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
