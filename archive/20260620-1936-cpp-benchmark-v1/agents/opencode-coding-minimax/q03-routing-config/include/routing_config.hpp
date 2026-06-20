#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

struct RouteRule {
    std::string prefix;
    std::string target;
};

struct Config {
    std::uint64_t version = 0;
    std::vector<RouteRule> routes;
};

class RoutingConfig {
public:
    using ObserverId = std::uint64_t;
    using Observer = std::function<void(std::shared_ptr<const Config>)>;

    explicit RoutingConfig(Config initial);
    const Config& current() const;
    std::shared_ptr<const Config> snapshot() const;
    bool reload(Config candidate);
    ObserverId subscribe(Observer observer);
    void unsubscribe(ObserverId id);
    std::string find_target(const std::string& path) const;
    std::size_t observer_error_count() const;

private:
    bool valid(const Config& candidate) const;

    struct State {
        std::shared_ptr<const Config> current;
    };

    struct ObserverEntry {
        std::shared_ptr<Observer> observer;
    };

    mutable std::shared_mutex mutex_;
    std::shared_ptr<State> state_;
    std::list<std::shared_ptr<const Config>> history_;
    std::map<ObserverId, ObserverEntry> observers_;
    ObserverId next_observer_id_ = 1;
    std::size_t observer_errors_ = 0;
};
