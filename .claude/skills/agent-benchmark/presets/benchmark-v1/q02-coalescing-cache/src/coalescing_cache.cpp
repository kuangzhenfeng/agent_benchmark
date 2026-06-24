#include "coalescing_cache.hpp"

#include <stdexcept>
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

struct CoalescingCache::State {
    explicit State(std::size_t capacity, Clock now)
        : capacity(capacity), now(std::move(now)) {}

    std::size_t capacity;
    Clock now;
};

CoalescingCache::CoalescingCache(std::size_t capacity, Clock now)
    : state_(std::make_shared<State>(capacity, std::move(now))) {}

LoadResult CoalescingCache::get_or_load(const std::string&, std::uint64_t,
                                        const Loader&) {
    return LoadResult::failed("not implemented");
}

void CoalescingCache::invalidate(const std::string&) {}

std::size_t CoalescingCache::size() const {
    return 0;
}
