#include "routing_config.hpp"

#include <algorithm>
#include <set>
#include <string_view>
#include <unordered_map>
#include <utility>

RoutingConfig::RoutingConfig(Config initial)
    : current_snapshot_(std::make_shared<const Config>(std::move(initial))) {}

const Config& RoutingConfig::current() const {
    thread_local std::unordered_map<const RoutingConfig*, std::shared_ptr<const Config>> pins;
    auto& pin = pins[this];

    std::shared_ptr<const Config> snap;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snap = current_snapshot_;
    }

    if (!pin || pin->version != snap->version) {
        pin = snap;
    }

    return *pin;
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
    if (!valid(candidate)) return false;

    std::shared_ptr<const Config> new_snapshot;
    std::vector<std::shared_ptr<Observer>> observer_list;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        candidate.version = current_snapshot_->version + 1;
        new_snapshot = std::make_shared<const Config>(std::move(candidate));
        current_snapshot_ = new_snapshot;

        for (const auto& [id, obs_ptr] : observers_) {
            observer_list.push_back(obs_ptr);
        }
    }

    for (const auto& obs_ptr : observer_list) {
        try {
            (*obs_ptr)(new_snapshot);
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex_);
            ++observer_errors_;
        }
    }

    return true;
}

RoutingConfig::ObserverId RoutingConfig::subscribe(Observer observer) {
    auto obs_ptr = std::make_shared<Observer>(std::move(observer));
    std::lock_guard<std::mutex> lock(mutex_);
    const ObserverId id = next_observer_id_++;
    observers_.emplace(id, std::move(obs_ptr));
    return id;
}

void RoutingConfig::unsubscribe(ObserverId id) {
    std::shared_ptr<Observer> obs_to_destroy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = observers_.find(id);
        if (it != observers_.end()) {
            obs_to_destroy = std::move(it->second);
            observers_.erase(it);
        }
    }
}

std::string RoutingConfig::find_target(const std::string& path) const {
    std::shared_ptr<const Config> snap;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snap = current_snapshot_;
    }
    const RouteRule* best = nullptr;
    for (const auto& route : snap->routes) {
        if (path.compare(0, route.prefix.size(), route.prefix) == 0 &&
            (best == nullptr || route.prefix.size() > best->prefix.size())) {
            best = &route;
        }
    }
    return best ? best->target : std::string{};
}

std::size_t RoutingConfig::observer_error_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return observer_errors_;
}
