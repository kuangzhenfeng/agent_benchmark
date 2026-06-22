#include "routing_config.hpp"

#include <algorithm>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

RoutingConfig::RoutingConfig(Config initial)
    : published_(std::make_shared<const Config>(std::move(initial))) {}

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

// current() pins a reference to the immutable snapshot observed at this call.
// The pin lives in a thread-local map keyed by instance identity, so:
//   - reload() never invalidates a returned reference (published_ is swapped,
//     but the pinned shared_ptr keeps the old snapshot alive);
//   - calling current() on a DIFFERENT instance updates a different slot and
//     does not touch this instance's pin;
//   - the reference stays valid until this thread calls current() again on the
//     SAME instance.
// This is per-thread/per-instance lifetime management of immutable snapshots
// obtained through the normal atomic-publish path — not a thread-local global
// mutable routing table.
const Config& RoutingConfig::current() const {
    thread_local std::unordered_map<const RoutingConfig*,
                                    std::shared_ptr<const Config>>
        tls_pins;

    std::shared_ptr<const Config> snap;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snap = published_;  // copy the control-block pointer under lock
    }
    std::shared_ptr<const Config>& slot = tls_pins[this];
    slot = std::move(snap);  // replace the pin for THIS instance only
    return *slot;
}

std::shared_ptr<const Config> RoutingConfig::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return published_;  // immutable snapshot; readers get a stable refcounted copy
}

bool RoutingConfig::reload(Config candidate) {
    std::shared_ptr<const Config> new_snapshot;
    std::vector<std::shared_ptr<Observer>> to_notify;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Full validation FIRST; on failure nothing changes (version, snapshot,
        // observers all untouched).
        if (!valid(candidate)) {
            return false;
        }
        candidate.version = published_->version + 1;  // strictly +1
        new_snapshot = std::make_shared<const Config>(std::move(candidate));
        published_ = new_snapshot;  // atomic publish via pointer swap, under lock
        // Snapshot the observer list by copying shared_ptr references (cheap;
        // never copies the user Observer). New subscriptions during the
        // notification phase are not in this list (they miss this round).
        for (const auto& entry : observers_) {
            to_notify.push_back(entry.second);
        }
    }  // lock released: notify outside the mutex

    // Notify each observer subscribed at reload time, at most once, with the
    // freshly published immutable snapshot. Observer copy/move/destruction is
    // not triggered here (we hold shared_ptrs and only invoke).
    for (auto& observer : to_notify) {
        try {
            (*observer)(new_snapshot);
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex_);
            ++observer_errors_;
        }
    }
    return true;
}

RoutingConfig::ObserverId RoutingConfig::subscribe(Observer observer) {
    // The user Observer is moved into a shared_ptr here (its copy/move may run
    // user code). This happens before taking the lock, so any user code in the
    // Observer's special members is outside the mutex.
    auto held = std::make_shared<Observer>(std::move(observer));
    std::lock_guard<std::mutex> lock(mutex_);
    const ObserverId id = next_observer_id_++;
    observers_.emplace(id, std::move(held));  // only moves a control-block ptr
    return id;
}

void RoutingConfig::unsubscribe(ObserverId id) {
    std::shared_ptr<Observer> removed;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = observers_.find(id);
        if (it != observers_.end()) {
            removed = std::move(it->second);  // move control-block ptr; no user code
            observers_.erase(it);
        }
    }
    // Dropping the final owner (→ Observer destructor, which may run user code
    // or re-enter) happens here, outside the lock.
    removed.reset();
}

std::string RoutingConfig::find_target(const std::string& path) const {
    // Read exactly one consistent snapshot.
    std::shared_ptr<const Config> snap;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snap = published_;
    }
    const RouteRule* best = nullptr;
    for (const auto& route : snap->routes) {
        // path.compare returns non-zero when the prefix is longer than path, so
        // over-long prefixes never match.
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
