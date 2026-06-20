#include "routing_config.hpp"

#include <algorithm>
#include <set>
#include <utility>

RoutingConfig::RoutingConfig(Config initial)
    : published_(std::make_shared<const Config>(std::move(initial))) {}

const Config& RoutingConfig::current() const {
    const auto tid = std::this_thread::get_id();
    std::lock_guard<std::mutex> lock(mutex_);
    pinned_[tid] = published_;
    return *pinned_[tid];
}

std::shared_ptr<const Config> RoutingConfig::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return published_;
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
    if (!valid(candidate)) {
        return false;
    }

    // 复制观察者列表（仅 shared_ptr，不复制 Observer 本身）
    std::vector<std::pair<ObserverId, std::shared_ptr<Observer>>> observers_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        candidate.version = published_->version + 1;
        auto config = std::make_shared<const Config>(std::move(candidate));
        published_ = std::move(config);
        observers_copy.assign(observers_.begin(), observers_.end());
    }

    // 锁外通知观察者
    std::size_t errors = 0;
    for (const auto& kv : observers_copy) {
        try {
            (*kv.second)(published_);
        } catch (...) {
            ++errors;
        }
    }

    if (errors > 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        observer_errors_ += errors;
    }

    return true;
}

RoutingConfig::ObserverId RoutingConfig::subscribe(Observer observer) {
    std::lock_guard<std::mutex> lock(mutex_);
    const ObserverId id = next_observer_id_++;
    observers_.emplace(id, std::make_shared<Observer>(std::move(observer)));
    return id;
}

void RoutingConfig::unsubscribe(ObserverId id) {
    std::shared_ptr<Observer> dying;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = observers_.find(id);
        if (it != observers_.end()) {
            dying = std::move(it->second);
            observers_.erase(it);
        }
    }
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