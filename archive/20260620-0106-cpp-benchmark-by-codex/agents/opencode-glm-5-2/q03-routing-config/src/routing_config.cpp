#include "routing_config.hpp"

#include <algorithm>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

RoutingConfig::RoutingConfig(Config initial)
    : current_config_(std::make_shared<const Config>(std::move(initial))) {}

const Config& RoutingConfig::current() const {
    thread_local std::unordered_map<const RoutingConfig*,
                                    std::shared_ptr<const Config>> pins;
    std::shared_ptr<const Config> snap;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        snap = current_config_;
    }
    auto& pin = pins[this];
    pin = std::move(snap);
    return *pin;
}

std::shared_ptr<const Config> RoutingConfig::snapshot() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return current_config_;
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

    std::shared_ptr<const Config> published;
    std::vector<std::pair<ObserverId, Observer>> to_notify;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        candidate.version = current_config_->version + 1;
        published = std::make_shared<const Config>(std::move(candidate));
        current_config_ = published;
        to_notify.assign(observers_.begin(), observers_.end());
    }

    std::size_t errors = 0;
    for (const auto& entry : to_notify) {
        try {
            entry.second(published);
        } catch (...) {
            ++errors;
        }
    }

    if (errors > 0) {
        std::lock_guard<std::mutex> lk(mutex_);
        observer_errors_ += errors;
    }

    return true;
}

RoutingConfig::ObserverId RoutingConfig::subscribe(Observer observer) {
    std::lock_guard<std::mutex> lk(mutex_);
    const ObserverId id = next_observer_id_++;
    observers_.emplace(id, std::move(observer));
    return id;
}

void RoutingConfig::unsubscribe(ObserverId id) {
    std::lock_guard<std::mutex> lk(mutex_);
    observers_.erase(id);
}

std::string RoutingConfig::find_target(const std::string& path) const {
    std::shared_ptr<const Config> snap;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        snap = current_config_;
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
    std::lock_guard<std::mutex> lk(mutex_);
    return observer_errors_;
}
