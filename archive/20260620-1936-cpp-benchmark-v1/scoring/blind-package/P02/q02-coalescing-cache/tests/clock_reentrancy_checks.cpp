#include "coalescing_cache.hpp"

#include <cassert>
#include <cstdint>

int main() {
    CoalescingCache* cache_ptr = nullptr;
    std::uint64_t tick = 100;
    CoalescingCache cache(2, [&] {
        assert(cache_ptr != nullptr);
        // Clock 的调用不能发生在 cache mutex 之下：invalidate 需要
        // 这把 mutex，而它是允许的公开重入操作。
        cache_ptr->invalidate("unrelated");
        return tick++;
    });
    cache_ptr = &cache;

    int loads = 0;
    const auto result = cache.get_or_load("profile:7", 10, [&loads](const std::string&) {
        ++loads;
        return LoadResult::ok("alice");
    });

    assert(result.status == LoadStatus::ok);
    assert(result.value == "alice");
    assert(loads == 1);
    assert(cache.size() == 1);
}
