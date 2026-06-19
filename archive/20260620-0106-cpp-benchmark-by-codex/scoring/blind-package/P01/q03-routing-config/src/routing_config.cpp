#include "routing_config.hpp"

#include <algorithm>
#include <set>
#include <utility>

RoutingConfig::RoutingConfig(Config initial)
    : current_snapshot_(std::make_shared<const Config>(std::move(initial))) {}

const Config& RoutingConfig::current() const {
    static thread_local std::shared_ptr<const Config> tls;
    std::shared_lock<std::shared_mutex> rlock(config_rwlock_);
    tls = current_snapshot_;
    return *tls;
}

std::shared_ptr<const Config> RoutingConfig::snapshot() const {
    std::shared_lock<std::shared_mutex> rlock(config_rwlock_);
    return current_snapshot_;
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

    std::shared_ptr<const Config> new_snapshot;
    std::map<ObserverId, Observer> observer_snap;

    {
        std::lock_guard<std::mutex> obs_lock(observer_mutex_);
        std::unique_lock<std::shared_mutex> cfg_wlock(config_rwlock_);

        std::uint64_t current_version = current_snapshot_ ? current_snapshot_->version : 0;
        candidate.version = current_version + 1;
        new_snapshot = std::make_shared<const Config>(std::move(candidate));
        current_snapshot_ = new_snapshot;

        observer_snap = observers_;
    }

    for (const auto& [id, observer] : observer_snap) {
        try {
            observer(new_snapshot);
        } catch (...) {
            std::lock_guard<std::mutex> obs_lock(observer_mutex_);
            ++observer_errors_;
        }
    }

    return true;
}

RoutingConfig::ObserverId RoutingConfig::subscribe(Observer observer) {
    std::lock_guard<std::mutex> lock(observer_mutex_);
    const ObserverId id = next_observer_id_++;
    observers_.emplace(id, std::move(observer));
    return id;
}

void RoutingConfig::unsubscribe(ObserverId id) {
    std::lock_guard<std::mutex> lock(observer_mutex_);
    observers_.erase(id);
}

std::string RoutingConfig::find_target(const std::string& path) const {
    std::shared_ptr<const Config> snap;
    {
        std::shared_lock<std::shared_mutex> rlock(config_rwlock_);
        snap = current_snapshot_;
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
    std::lock_guard<std::mutex> lock(observer_mutex_);
    return observer_errors_;
}
