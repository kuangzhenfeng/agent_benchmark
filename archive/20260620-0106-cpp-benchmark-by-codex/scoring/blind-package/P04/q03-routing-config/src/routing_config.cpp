#include "routing_config.hpp"

#include <algorithm>
#include <set>
#include <utility>

RoutingConfig::RoutingConfig(Config initial)
    : current_snapshot_(std::make_shared<const Config>(std::move(initial))) {}

const Config& RoutingConfig::current() const {
    // thread_local shared_ptr keeps the Config alive until this thread's
    // next call to current(). The shared_ptr prevents the referenced Config
    // from being destroyed by concurrent reload() calls.
    thread_local std::shared_ptr<const Config> tls_snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tls_snapshot = current_snapshot_;
    }
    return *tls_snapshot;
}

std::shared_ptr<const Config> RoutingConfig::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
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
    std::map<ObserverId, Observer> observers_snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        candidate.version = current_snapshot_->version + 1;
        new_snapshot = std::make_shared<const Config>(std::move(candidate));
        current_snapshot_ = new_snapshot;
        observers_snapshot = observers_;
    }

    // Notify observers OUTSIDE the lock – observers may re-enter
    // subscribe, unsubscribe, reload, snapshot, current, find_target.
    for (const auto& [id, observer] : observers_snapshot) {
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
    std::shared_ptr<const Config> snap;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snap = current_snapshot_;
    }
    // Search on a consistent immutable snapshot, no lock needed.
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
