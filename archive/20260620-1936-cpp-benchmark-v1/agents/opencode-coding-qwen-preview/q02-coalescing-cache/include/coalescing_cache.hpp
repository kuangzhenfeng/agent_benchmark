#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

enum class LoadStatus {
    ok,
    loader_failed,
    recursive_load,
};

struct LoadResult {
    LoadStatus status = LoadStatus::loader_failed;
    std::string value;
    std::string error;

    static LoadResult ok(std::string value);
    static LoadResult failed(std::string error);
    static LoadResult recursive();
};

class CoalescingCache {
public:
    using Clock = std::function<std::uint64_t()>;
    using Loader = std::function<LoadResult(const std::string& key)>;

    CoalescingCache(std::size_t capacity, Clock now);
    LoadResult get_or_load(const std::string& key, std::uint64_t ttl,
                           const Loader& loader);
    void invalidate(const std::string& key);
    std::size_t size() const;

private:
    struct State;
    std::shared_ptr<State> state_;
};
