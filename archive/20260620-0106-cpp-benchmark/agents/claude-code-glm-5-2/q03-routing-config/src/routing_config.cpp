#include "routing_config.hpp"

#include <algorithm>
#include <set>
#include <utility>

RoutingConfig::RoutingConfig(Config initial)
    : current_(std::make_shared<const Config>(std::move(initial))) {}

const Config& RoutingConfig::current() const {
    // current() 必须返回稳定的引用：在「该调用线程下一次调用 current() 之前」
    // 以及「之后的任意 reload 中」保持有效。
    //
    // 策略：每个线程持有一个 thread_local 的 shared_ptr，锁定当前已发布的配置，
    // 由此 pin 住底层 Config 对象，使其在 reload 替换 current_ 后不会被释放。
    // 引用指向被 pin 住的对象，直到该线程下次调用 current()（或线程退出）。
    thread_local std::shared_ptr<const Config> thread_pin;

    std::shared_ptr<const Config> latest = [this] {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_;
    }();
    // 切换本线程的 pin：旧的引用让位给新的，但只要不再调用 current()，
    // 返回的引用始终指向 latest 这份被本线程 pin 住的对象。
    thread_pin = latest;
    return *latest;
}

std::shared_ptr<const Config> RoutingConfig::snapshot() const {
    // 返回不可变、完整自洽的快照。短锁拷贝 shared_ptr（仅增加引用计数），
    // reader 之间互不阻塞，且永远拿到一个完整对象。
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}

bool RoutingConfig::valid(const Config& candidate) const {
    std::set<std::string> prefixes;
    for (const auto& route : candidate.routes) {
        if (route.prefix.empty() || route.target.empty() ||
            !prefixes.insert(route.prefix).second) {
            return false;
        }
    }
    return true;
}

bool RoutingConfig::reload(Config candidate) {
    // 1) 先完整校验，失败则快照/版本/观察者均不变。
    if (!valid(candidate)) {
        return false;
    }

    std::shared_ptr<const Config> published;
    std::vector<std::pair<ObserverId, Observer>> to_notify;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // 版本严格加一（基于当前已发布版本）。
        candidate.version = current_->version + 1;
        published = std::make_shared<const Config>(std::move(candidate));
        // 一次性发布：原子替换 current_。reader 此刻起要么看到旧对象要么看到新对象，
        // 不会看到半更新。current()/snapshot() 通过 shared_ptr 共享同一对象。
        current_ = published;
        // 通知快照时刻：取「调用开始时仍已订阅」的观察者副本。
        to_notify.reserve(observers_.size());
        for (const auto& kv : observers_) {
            to_notify.emplace_back(kv.first, kv.second);
        }
    }

    // 2) 在内部互斥锁之外通知。观察者可重入 subscribe/unsubscribe/reload/snapshot。
    //    异常隔离：单个观察者抛异常不阻止其余，且不影响已发布配置（无需回滚）。
    for (const auto& kv : to_notify) {
        try {
            (kv.second)(published);
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex_);
            ++observer_errors_;
        }
    }
    return true;
}

RoutingConfig::ObserverId RoutingConfig::subscribe(Observer observer) {
    std::lock_guard<std::mutex> lock(mutex_);
    const ObserverId id = next_observer_id_++;
    observers_.emplace(id, std::move(observer));
    return id;
}

void RoutingConfig::unsubscribe(ObserverId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    observers_.erase(id);
}

std::string RoutingConfig::find_target(const std::string& path) const {
    // 只基于一个一致快照：拷贝一份 shared_ptr 后在锁外查询，期间该快照不可变。
    std::shared_ptr<const Config> snap;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snap = current_;
    }
    const RouteRule* best = nullptr;
    for (const auto& route : snap->routes) {
        if (path.compare(0, route.prefix.size(), route.prefix) == 0 &&
            (best == nullptr || route.prefix.size() > best->prefix.size())) {
            best = &route;
        }
    }
    return best == nullptr ? std::string{} : best->target;
}

std::size_t RoutingConfig::observer_error_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return observer_errors_;
}
