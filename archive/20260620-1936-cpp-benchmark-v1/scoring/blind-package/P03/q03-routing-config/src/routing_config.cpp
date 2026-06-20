#include "routing_config.hpp"

#include <algorithm>
#include <set>
#include <utility>

// 每个实例的 TLS pin 槽：使用实例地址作为 key 的 thread_local map
struct TlsPinMap {
    thread_local static std::map<const RoutingConfig*, std::shared_ptr<const Config>> pins;
};

thread_local std::map<const RoutingConfig*, std::shared_ptr<const Config>> TlsPinMap::pins;

RoutingConfig::RoutingConfig(Config initial)
    : current_(std::make_shared<const Config>(std::move(initial))) {}

RoutingConfig::~RoutingConfig() {
    // 清理该实例在 TLS 中的 pin
    TlsPinMap::pins.erase(this);
}

const Config& RoutingConfig::current() const {
    std::lock_guard<std::mutex> lock(mutex_);
    TlsPinMap::pins[this] = current_;
    return *current_;
}

std::shared_ptr<const Config> RoutingConfig::snapshot() const {
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
    // 1. 校验候选配置
    if (!valid(candidate)) {
        return false;
    }

    // 2. 在锁内原子替换配置并收集观察者
    std::vector<Observer> observers;
    std::shared_ptr<const Config> new_config;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        candidate.version = current_->version + 1;
        new_config = std::make_shared<const Config>(std::move(candidate));
        current_ = new_config;
        for (const auto& observer : observers_) {
            observers.push_back(observer.second);
        }
    }

    // 3. 在锁外通知观察者
    for (const auto& observer : observers) {
        try {
            observer(new_config);
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
    std::shared_ptr<const Config> config = snapshot();
    const RouteRule* best = nullptr;
    for (const auto& route : config->routes) {
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
