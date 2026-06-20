#include "routing_config.hpp"

#include <algorithm>
#include <set>
#include <utility>

RoutingConfig::RoutingConfig(Config initial)
    : current_snapshot_(std::make_shared<const Config>(std::move(initial))) {}

const Config& RoutingConfig::current() const {
    std::shared_ptr<const Config> snap;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snap = current_snapshot_;
    }
    {
        std::lock_guard<std::mutex> lock(pin_mutex_);
        thread_pins_[std::this_thread::get_id()] = snap;
    }
    return *snap;
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
    std::vector<std::pair<ObserverId, std::shared_ptr<Observer>>> observers_snapshot;
    std::shared_ptr<const Config> published;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!valid(candidate)) {
            return false;
        }
        candidate.version = current_snapshot_->version + 1;
        published = std::make_shared<const Config>(std::move(candidate));
        current_snapshot_ = published;
        observers_snapshot.reserve(observers_.size());
        for (const auto& entry : observers_) {
            observers_snapshot.push_back(entry);
        }
    }

    for (const auto& entry : observers_snapshot) {
        try {
            (*entry.second)(published);
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
    std::shared_ptr<Observer> to_destroy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = observers_.find(id);
        if (it != observers_.end()) {
            to_destroy = std::move(it->second);
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
