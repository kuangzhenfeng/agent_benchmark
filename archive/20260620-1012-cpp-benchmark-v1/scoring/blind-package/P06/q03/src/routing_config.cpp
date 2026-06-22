#include "routing_config.hpp"

#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using CurrentPins =
    std::unordered_map<const RoutingConfig*, std::shared_ptr<const Config>>;

CurrentPins& current_pins() {
    thread_local CurrentPins pins;
    return pins;
}

}  // namespace

RoutingConfig::RoutingConfig(Config initial)
    : current_snapshot_(std::make_shared<const Config>(std::move(initial))) {}

const Config& RoutingConfig::current() const {
    auto snapshot_ref = snapshot();
    auto& pins = current_pins();
    auto it = pins.find(this);
    if (it == pins.end()) {
        it = pins.emplace(this, std::move(snapshot_ref)).first;
    } else {
        it->second = std::move(snapshot_ref);
    }
    return *it->second;
}

std::shared_ptr<const Config> RoutingConfig::snapshot() const {
    return std::atomic_load_explicit(&current_snapshot_, std::memory_order_acquire);
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
    std::vector<std::shared_ptr<ObserverSlot>> observers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto current_snapshot =
            std::atomic_load_explicit(&current_snapshot_, std::memory_order_relaxed);
        candidate.version = current_snapshot->version + 1;
        published = std::make_shared<const Config>(std::move(candidate));

        observers.reserve(observers_.size());
        for (const auto& entry : observers_) {
            observers.push_back(entry.second);
        }

        std::atomic_store_explicit(&current_snapshot_, published, std::memory_order_release);
    }

    for (const auto& observer : observers) {
        try {
            observer->observer(published);
        } catch (...) {
            observer_errors_.fetch_add(1, std::memory_order_relaxed);
        }
    }
    return true;
}

RoutingConfig::ObserverId RoutingConfig::subscribe(Observer observer) {
    auto slot = std::make_shared<ObserverSlot>(ObserverSlot{std::move(observer)});

    std::lock_guard<std::mutex> lock(mutex_);
    const ObserverId id = next_observer_id_++;
    observers_.emplace(id, std::move(slot));
    return id;
}

void RoutingConfig::unsubscribe(ObserverId id) {
    std::shared_ptr<ObserverSlot> retired;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = observers_.find(id);
        if (it == observers_.end()) {
            return;
        }
        retired = std::move(it->second);
        observers_.erase(it);
    }
}

std::string RoutingConfig::find_target(const std::string& path) const {
    const auto config = snapshot();

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
    return observer_errors_.load(std::memory_order_relaxed);
}
