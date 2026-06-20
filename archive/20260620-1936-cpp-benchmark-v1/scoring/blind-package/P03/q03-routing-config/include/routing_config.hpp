#pragma once

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
    ~RoutingConfig();
    const Config& current() const;
    std::shared_ptr<const Config> snapshot() const;
    bool reload(Config candidate);
    ObserverId subscribe(Observer observer);
    void unsubscribe(ObserverId id);
    std::string find_target(const std::string& path) const;
    std::size_t observer_error_count() const;

private:
    bool valid(const Config& candidate) const;

    mutable std::mutex mutex_;
    std::shared_ptr<const Config> current_;
    std::map<ObserverId, Observer> observers_;
    ObserverId next_observer_id_ = 1;
    std::size_t observer_errors_ = 0;

    // 每个实例的 TLS pin 槽，用于 current() 返回的引用保活
    struct TlsPin;
    std::shared_ptr<TlsPin> tls_pin_;
};
