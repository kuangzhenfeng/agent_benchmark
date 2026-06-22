#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
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
    struct ObserverSlot {
        Observer observer;
    };

    bool valid(const Config& candidate) const;

    mutable std::mutex mutex_;
    std::shared_ptr<const Config> current_snapshot_;
    std::map<ObserverId, std::shared_ptr<ObserverSlot>> observers_;
    ObserverId next_observer_id_ = 1;
    std::atomic<std::size_t> observer_errors_{0};
};
