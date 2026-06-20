#include "routing_config.hpp"

#include <algorithm>
#include <set>
#include <utility>

RoutingConfig::RoutingConfig(Config initial) {
    initial.version = 0;
    auto snap = std::make_shared<const Config>(std::move(initial));
    history_.push_back(std::move(snap));
    published_ptr_ = history_.back().get();
}

const Config& RoutingConfig::current() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return *published_ptr_;
}

std::shared_ptr<const Config> RoutingConfig::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return history_.back();
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
    std::shared_ptr<const Config> new_snapshot;
    std::vector<Observer> observers_to_notify;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!valid(candidate)) {
            return false;
        }
        candidate.version = history_.back()->version + 1;
        new_snapshot = std::make_shared<const Config>(std::move(candidate));
        history_.push_back(new_snapshot);
        published_ptr_ = history_.back().get();

        // 复制观察者列表到锁外调用
        for (const auto& obs : observers_) {
            observers_to_notify.push_back(obs.second);
        }
    }

    // 在锁外通知观察者
    for (const auto& observer : observers_to_notify) {
        try {
            observer(new_snapshot);
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
    auto snap = snapshot();
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
