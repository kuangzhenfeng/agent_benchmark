#include "routing_config.hpp"

#include <algorithm>
#include <set>
#include <unordered_map>
#include <utility>

RoutingConfig::RoutingConfig(Config initial)
    : current_(std::make_shared<const Config>(std::move(initial))) {}

const Config& RoutingConfig::current() const {
    thread_local std::unordered_map<const void*, std::shared_ptr<const Config>> pins;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pins[this] = current_;
    }
    return *pins[this];
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
    if (!valid(candidate)) return false;

    std::shared_ptr<const Config> published;
    std::map<ObserverId, std::shared_ptr<Observer>> observers_snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        candidate.version = current_->version + 1;
        published = std::make_shared<const Config>(std::move(candidate));
        current_ = published;
        observers_snapshot = observers_;
    }

    for (const auto& [id, observer] : observers_snapshot) {
        try {
            (*observer)(published);
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
    observers_.emplace(id, std::make_shared<Observer>(std::move(observer)));
    return id;
}

void RoutingConfig::unsubscribe(ObserverId id) {
    std::shared_ptr<Observer> removed;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = observers_.find(id);
        if (it == observers_.end()) return;
        removed = std::move(it->second);
        observers_.erase(it);
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
