#include "coalescing_cache.hpp"

#include <algorithm>
#include <thread>
#include <utility>

LoadResult LoadResult::ok(std::string value) {
    return LoadResult{LoadStatus::ok, std::move(value), {}};
}

LoadResult LoadResult::failed(std::string error) {
    return LoadResult{LoadStatus::loader_failed, {}, std::move(error)};
}

LoadResult LoadResult::recursive() {
    return LoadResult{LoadStatus::recursive_load, {}, "recursive load"};
}

CoalescingCache::CoalescingCache(std::size_t capacity, Clock now)
    : capacity_(capacity), now_(std::move(now)) {}

LoadResult CoalescingCache::get_or_load(const std::string& key, std::uint64_t ttl,
                                        const Loader& loader) {
    const auto tid = std::this_thread::get_id();

    // 递归检测（锁外，使用单独的保护）
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = loading_keys_.find(tid);
        if (it != loading_keys_.end()) {
            for (const auto& k : it->second) {
                if (k == key) {
                    return LoadResult::recursive();
                }
            }
        }
    }

    // 调用 Clock（锁外）
    const std::uint64_t current_tick = now_();

    // 尝试在 ready cache 中查找
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = ready_index_.find(key);
        if (it != ready_index_.end()) {
            auto& entry = *it->second;
            if (current_tick < entry.expire_at) {
                // 未过期，刷新 LRU 并返回
                lru_list_.splice(lru_list_.end(), lru_list_, it->second);
                return LoadResult::ok(entry.value);
            } else {
                // 已过期，移除
                lru_list_.erase(it->second);
                ready_index_.erase(it);
            }
        }
    }

    // 尝试加入在途加载或创建新的
    std::shared_ptr<InFlightState> flight;
    bool is_owner = false;

    {
        std::unique_lock<std::mutex> lock(mutex_);

        // 再次检查 ready（可能在上面两次锁之间有其他线程写入了）
        auto it = ready_index_.find(key);
        if (it != ready_index_.end()) {
            auto& entry = *it->second;
            if (current_tick < entry.expire_at) {
                lru_list_.splice(lru_list_.end(), lru_list_, it->second);
                return LoadResult::ok(entry.value);
            } else {
                lru_list_.erase(it->second);
                ready_index_.erase(it);
            }
        }

        // 检查是否在途
        auto flight_it = in_flight_.find(key);
        if (flight_it != in_flight_.end()) {
            flight = flight_it->second.state;
            // 作为等待者，在锁外等待
        } else {
            // 创建新的 flight
            flight = std::make_shared<InFlightState>();
            in_flight_[key] = FlightInfo{flight, false};
            is_owner = true;
            loading_keys_[tid].push_back(key);
        }
    }

    if (!is_owner) {
        // 等待者：等待 flight 完成
        std::unique_lock<std::mutex> fl(flight->mutex);
        flight->cv.wait(fl, [&flight] { return flight->done; });
        return flight->result;
    }

    // Owner：执行 loader（锁已释放）
    LoadResult result;
    try {
        result = loader(key);
    } catch (...) {
        result = LoadResult::failed("loader exception");
    }

    const std::uint64_t load_complete_tick = now_();

    {
        std::lock_guard<std::mutex> lock(mutex_);

        // 检查是否被 invalidate
        auto fl_it = in_flight_.find(key);
        bool invalidated = (fl_it == in_flight_.end()) || fl_it->second.invalidated;

        // 清除 loading_keys_
        auto lk_it = loading_keys_.find(tid);
        if (lk_it != loading_keys_.end()) {
            auto& keys = lk_it->second;
            keys.erase(std::remove(keys.begin(), keys.end(), key), keys.end());
            if (keys.empty()) {
                loading_keys_.erase(lk_it);
            }
        }

        // 完成 flight：设置结果、可能写缓存、移除 in_flight
        {
            std::unique_lock<std::mutex> fl_lock(flight->mutex);
            flight->result = result;
            flight->done = true;
        }
        flight->cv.notify_all();

        // 如果未 invalidate 且成功，写入 ready cache
        if (!invalidated && result.status == LoadStatus::ok && capacity_ > 0) {
            // LRU 淘汰
            while (ready_index_.size() >= capacity_ && !lru_list_.empty()) {
                const auto& evict = lru_list_.front();
                ready_index_.erase(evict.key);
                lru_list_.pop_front();
            }
            CacheEntry entry{key, result.value, load_complete_tick + ttl};
            lru_list_.push_back(entry);
            ready_index_[key] = std::prev(lru_list_.end());
        }

        // 移除 in_flight
        in_flight_.erase(key);
    }

    return result;
}

void CoalescingCache::invalidate(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 从 ready 移除
    auto it = ready_index_.find(key);
    if (it != ready_index_.end()) {
        lru_list_.erase(it->second);
        ready_index_.erase(it);
    }

    // 标记在途加载为 invalidated（loader 完成后不写回）
    auto fl_it = in_flight_.find(key);
    if (fl_it != in_flight_.end()) {
        fl_it->second.invalidated = true;
    }
}

std::size_t CoalescingCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ready_index_.size();
}
