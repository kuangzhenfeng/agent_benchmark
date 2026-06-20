#include "routing_config.hpp"

#include <algorithm>
#include <set>
#include <shared_mutex>
#include <utility>

namespace {

constexpr std::size_t MAX_HISTORY_SIZE = 16;

}  // namespace

RoutingConfig::RoutingConfig(Config initial)
    : state_(std::make_shared<State>(State{std::make_shared<const Config>(std::move(initial))})) {}

const Config& RoutingConfig::current() const {
    std::shared_lock<std::shared_mutex> read_lock(mutex_);
    return *state_->current;
}

std::shared_ptr<const Config> RoutingConfig::snapshot() const {
    std::shared_lock<std::shared_mutex> read_lock(mutex_);
    return state_->current;
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
    std::shared_ptr<const Config> published;
    std::vector<std::shared_ptr<Observer>> observer_ptrs;

    {
        std::unique_lock<std::shared_mutex> write_lock(mutex_);
        if (!valid(candidate)) {
            return false;
        }
        candidate.version = state_->current->version + 1;
        auto new_config = std::make_shared<const Config>(std::move(candidate));

        history_.push_front(state_->current);
        if (history_.size() > MAX_HISTORY_SIZE) {
            history_.pop_back();
        }

        state_ = std::make_shared<State>(State{new_config});
        published = new_config;

        for (auto& entry : observers_) {
            observer_ptrs.push_back(entry.second.observer);
        }
    }

    for (auto& obs_ptr : observer_ptrs) {
        try {
            (*obs_ptr)(published);
        } catch (...) {
            std::unique_lock<std::shared_mutex> write_lock(mutex_);
            ++observer_errors_;
        }
    }

    return true;
}

RoutingConfig::ObserverId RoutingConfig::subscribe(Observer observer) {
    std::unique_lock<std::shared_mutex> write_lock(mutex_);
    const ObserverId id = next_observer_id_++;
    observers_.emplace(id, ObserverEntry{std::make_shared<Observer>(std::move(observer))});
    return id;
}

void RoutingConfig::unsubscribe(ObserverId id) {
    std::unique_lock<std::shared_mutex> write_lock(mutex_);
    observers_.erase(id);
}

std::string RoutingConfig::find_target(const std::string& path) const {
    std::shared_lock<std::shared_mutex> read_lock(mutex_);
    const auto& current = *state_->current;
    const RouteRule* best = nullptr;
    for (const auto& route : current.routes) {
        if (path.compare(0, route.prefix.size(), route.prefix) == 0 &&
            (best == nullptr || route.prefix.size() > best->prefix.size())) {
            best = &route;
        }
    }
    return best == nullptr ? std::string{} : best->target;
}

std::size_t RoutingConfig::observer_error_count() const {
    std::shared_lock<std::shared_mutex> read_lock(mutex_);
    return observer_errors_;
}
