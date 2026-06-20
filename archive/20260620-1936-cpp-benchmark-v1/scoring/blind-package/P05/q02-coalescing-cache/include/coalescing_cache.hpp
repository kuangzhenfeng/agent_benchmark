#pragma once

#include <cstddef>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

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
    struct InFlightState {
        std::mutex mutex;
        std::condition_variable cv;
        bool done = false;
        LoadResult result;
    };

    struct CacheEntry {
        std::string key;
        std::string value;
        std::uint64_t expire_at;
    };

    mutable std::mutex mutex_;
    const std::size_t capacity_;
    const Clock now_;

    // LRU list: front = least recently used
    std::list<CacheEntry> lru_list_;
    std::unordered_map<std::string, std::list<CacheEntry>::iterator> ready_index_;

    // 在途加载：key -> shared state
    struct FlightInfo {
        std::shared_ptr<InFlightState> state;
        bool invalidated = false;  // loader 运行期间被 invalidate
    };
    std::unordered_map<std::string, FlightInfo> in_flight_;

    // 递归检测：每线程 key 栈
    std::unordered_map<std::thread::id, std::vector<std::string>> loading_keys_;
};
