#include "routing_config.hpp"

#include <algorithm>
#include <set>
#include <utility>

namespace {

// This is only a per-thread ownership slot for the legacy reference-returning
// API.  It is not used to publish routing state: publication remains in the
// instance-owned shared snapshot below.  A single slot also bounds retained
// snapshots to the lifetime promised by current(): until the thread's next
// current() call.
thread_local std::shared_ptr<const Config> current_reference_holder;

}  // namespace

RoutingConfig::RoutingConfig(Config initial)
    : current_(std::make_shared<const Config>(std::move(initial))) {}

const Config& RoutingConfig::current() const {
    current_reference_holder = snapshot();
    return *current_reference_holder;
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
    std::vector<std::shared_ptr<const ObserverEntry>> observer_snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        candidate.version = current_->version + 1;
        published = std::make_shared<const Config>(std::move(candidate));

        // Build the notification set at the same synchronization point as
        // publication.  This makes subscribe/unsubscribe affect only future
        // reloads once this block finishes.
        observer_snapshot.reserve(observers_.size());
        for (const auto& observer : observers_) {
            observer_snapshot.push_back(observer.second);
        }
        current_ = published;
    }

    for (const auto& observer : observer_snapshot) {
        try {
            observer->callback(published);
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex_);
            ++observer_errors_;
        }
    }
    return true;
}

RoutingConfig::ObserverId RoutingConfig::subscribe(Observer observer) {
    auto entry = std::make_shared<const ObserverEntry>(
        ObserverEntry{std::move(observer)});
    std::lock_guard<std::mutex> lock(mutex_);
    const ObserverId id = next_observer_id_++;
    observers_.emplace(id, std::move(entry));
    return id;
}

void RoutingConfig::unsubscribe(ObserverId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    observers_.erase(id);
}

std::string RoutingConfig::find_target(const std::string& path) const {
    const auto published = snapshot();
    const RouteRule* best = nullptr;
    for (const auto& route : published->routes) {
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
