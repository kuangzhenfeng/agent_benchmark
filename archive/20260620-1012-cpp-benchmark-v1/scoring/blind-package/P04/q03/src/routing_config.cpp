#include "routing_config.hpp"

#include <algorithm>
#include <set>
#include <unordered_map>
#include <utility>

namespace {

// Per-thread, per-instance lifetime pin for the legacy `const Config&
// current()` API. Keyed by instance address so that calling current() on a
// DIFFERENT RoutingConfig does not release the previous instance's pin. This
// holds only immutable Config refcounts (not a route table, not shared across
// threads), satisfying the constraint against thread-local shared mutable
// routing data.
thread_local std::unordered_map<const RoutingConfig*,
                                std::shared_ptr<const Config>>
    current_pins;

}  // namespace

RoutingConfig::RoutingConfig(Config initial) {
    // Keep the caller-supplied version (tests expect 0 before first reload).
    current_ = std::make_shared<const Config>(std::move(initial));
}

const Config& RoutingConfig::current() const {
    std::shared_ptr<const Config> snap;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snap = current_;  // refcount bump only, no user code
    }
    // Replace this thread's pin for THIS instance; the previous snapshot is
    // released (its Config stays alive while any reader holds a reference).
    std::shared_ptr<const Config>& pin = current_pins[this];
    pin = std::move(snap);
    return *pin;
}

std::shared_ptr<const Config> RoutingConfig::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;  // immutable, self-consistent snapshot
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
    // Validate fully BEFORE touching any state.
    if (!valid(candidate)) {
        return false;
    }

    std::shared_ptr<const Config> next;
    std::vector<std::shared_ptr<Observer>> to_notify;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Assign version and build the immutable snapshot to publish.
        candidate.version = current_->version + 1;
        next = std::make_shared<const Config>(std::move(candidate));
        current_ = next;  // atomic publish: readers never see a partial update

        // Snapshot observers subscribed at reload start. We copy shared_ptrs
        // (refcount bumps only); observers subscribed later by a callback are
        // not in this list and won't receive the current notification.
        to_notify.reserve(observers_.size());
        for (const auto& entry : observers_) {
            to_notify.push_back(entry.second);
        }
    }

    // Notify entirely outside the mutex so observers may re-enter any public
    // method without deadlock.
    notify_observers(next, std::move(to_notify));
    return true;
}

void RoutingConfig::notify_observers(
    std::shared_ptr<const Config> snapshot,
    std::vector<std::shared_ptr<Observer>> to_notify) {
    for (const auto& box : to_notify) {
        if (!box) continue;
        try {
            // Invoke without copying the std::function; user copy/move/destroy
            // of the Observer only happens via the shared_ptr refcount (here,
            // outside the lock).
            (*box)(snapshot);
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex_);
            ++observer_errors_;
        }
    }
    // `to_notify` and `snapshot` drop here, outside the lock. If an observer
    // was unsubscribed during notification and this was its last owner, the
    // user destructor runs here, safely outside the mutex.
}

RoutingConfig::ObserverId RoutingConfig::subscribe(Observer observer) {
    // Construct the holder outside the lock (Observer move is user code).
    auto box = std::make_shared<Observer>(std::move(observer));
    std::lock_guard<std::mutex> lock(mutex_);
    const ObserverId id = next_observer_id_++;
    observers_.emplace(id, std::move(box));  // shared_ptr move, no user code
    return id;
}

void RoutingConfig::unsubscribe(ObserverId id) {
    std::shared_ptr<Observer> removed;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = observers_.find(id);
        if (it != observers_.end()) {
            removed = std::move(it->second);  // pull out; entry now empty
            observers_.erase(it);  // destroys empty shared_ptr: no user code
        }
    }
    // `removed` drops here, outside the lock.
}

std::string RoutingConfig::find_target(const std::string& path) const {
    std::shared_ptr<const Config> snap;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snap = current_;
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
    std::lock_guard<std::mutex> lock(mutex_);
    return observer_errors_;
}
