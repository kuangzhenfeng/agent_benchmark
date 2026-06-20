#include "routing_config.hpp"

#include <algorithm>
#include <set>
#include <unordered_map>
#include <utility>

namespace {

// current() has a reference-returning compatibility API.  Each thread keeps
// one immutable snapshot per RoutingConfig instance, rather than one shared
// TLS slot, so a current() call on another instance cannot invalidate an
// earlier reference.
thread_local std::unordered_map<const RoutingConfig*, std::shared_ptr<const Config>>
    current_pins;

}  // namespace

RoutingConfig::RoutingConfig(Config initial)
    : current_(std::make_shared<const Config>(std::move(initial))) {}

const Config& RoutingConfig::current() const {
    std::shared_ptr<const Config> held;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        held = current_;
    }
    std::shared_ptr<const Config>& pin = current_pins[this];
    pin = std::move(held);
    return *pin;
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
    if (!valid(candidate)) {
        return false;
    }

    std::shared_ptr<const Config> published;
    std::vector<std::shared_ptr<Observer>> observers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        candidate.version = current_->version + 1;
        published = std::make_shared<const Config>(std::move(candidate));
        current_ = published;

        // The pointer snapshot defines who receives this notification.  No
        // Observer itself is copied while mutex_ is locked.
        observers.reserve(observers_.size());
        for (const auto& entry : observers_) {
            observers.push_back(entry.second);
        }
    }

    for (const std::shared_ptr<Observer>& observer : observers) {
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
    // std::function move/copy can invoke user special members; construct the
    // separately-owned callable before touching mutex_.
    auto observer_owner = std::make_shared<Observer>(std::move(observer));
    ObserverId id;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        id = next_observer_id_++;
        observers_.emplace(id, std::move(observer_owner));
    }
    return id;
}

void RoutingConfig::unsubscribe(ObserverId id) {
    std::shared_ptr<Observer> retired;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found = observers_.find(id);
        if (found == observers_.end()) {
            return;
        }
        retired = std::move(found->second);
        observers_.erase(found);
    }
}

std::string RoutingConfig::find_target(const std::string& path) const {
    const std::shared_ptr<const Config> held = snapshot();
    const RouteRule* best = nullptr;
    for (const auto& route : held->routes) {
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
