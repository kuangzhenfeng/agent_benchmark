#include "routing_config.hpp"

#include <algorithm>
#include <set>
#include <unordered_map>
#include <utility>

// ---------------------------------------------------------------------------
// Thread-local snapshot pin for current().
//
// Each thread maintains a per-instance cache of the last snapshot returned by
// current().  Calling current() on instance A updates only A's entry; calling
// current() on instance B does not affect A's pinned reference.
//
// This satisfies the cross-instance lifetime requirement (test
// cross_instance_current_checks.cpp) and the reload-survival requirement:
// the pinned shared_ptr keeps the old Config alive even after reload replaces
// current_snapshot_.
// ---------------------------------------------------------------------------

namespace {
thread_local std::unordered_map<const RoutingConfig*,
                                std::shared_ptr<const Config>>
    tls_pins;
}  // namespace

// ---------------------------------------------------------------------------

RoutingConfig::RoutingConfig(Config initial)
    : current_snapshot_(std::make_shared<const Config>(std::move(initial))) {}

const Config& RoutingConfig::current() const {
    std::shared_ptr<const Config> snap;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snap = current_snapshot_;
    }
    // Pin in TLS — keeps the snapshot alive until this thread calls current()
    // on the same instance again (which overwrites the entry).
    tls_pins[this] = snap;
    return *snap;
}

std::shared_ptr<const Config> RoutingConfig::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_snapshot_;
}

bool RoutingConfig::valid(const Config& candidate) {
    std::set<std::string> prefixes;
    for (const auto& route : candidate.routes) {
        if (route.prefix.empty() || route.target.empty()) return false;
        if (!prefixes.insert(route.prefix).second) return false;
    }
    return true;
}

bool RoutingConfig::reload(Config candidate) {
    // Phase 1: validate OUTSIDE lock (pure function, no shared state).
    if (!valid(candidate)) {
        return false;
    }

    // Phase 2: atomic publish under lock.
    std::shared_ptr<const Config> published;
    std::vector<std::shared_ptr<Observer>> observer_copies;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        candidate.version = current_snapshot_->version + 1;
        published = std::make_shared<const Config>(std::move(candidate));
        current_snapshot_ = published;

        // Copy observer shared_ptrs — increment refcount under lock.
        observer_copies.reserve(observers_.size());
        for (auto& [id, obs_ptr] : observers_) {
            observer_copies.push_back(obs_ptr);
        }
    }
    // mutex_ released.

    // Phase 3: notify observers OUTSIDE lock (requirement 4).
    for (auto& obs_ptr : observer_copies) {
        try {
            (*obs_ptr)(published);
        } catch (...) {
            // Exception isolation — increment error count under lock.
            std::lock_guard<std::mutex> lock(mutex_);
            ++observer_errors_;
        }
    }

    // observer_copies destroyed here — Observer callable destruction
    // (including last-owner-triggered destructor) happens outside lock.

    return true;
}

RoutingConfig::ObserverId RoutingConfig::subscribe(Observer observer) {
    std::lock_guard<std::mutex> lock(mutex_);
    const ObserverId id = next_observer_id_++;
    observers_.emplace(id, std::make_shared<Observer>(std::move(observer)));
    return id;
}

void RoutingConfig::unsubscribe(ObserverId id) {
    // Move out under lock, destroy outside lock.
    std::shared_ptr<Observer> removed;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = observers_.find(id);
        if (it != observers_.end()) {
            removed = std::move(it->second);
            observers_.erase(it);
        }
    }
    // `removed` destroyed here — outside lock.
}

std::string RoutingConfig::find_target(const std::string& path) const {
    // Take a consistent snapshot under lock, then search outside.
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
