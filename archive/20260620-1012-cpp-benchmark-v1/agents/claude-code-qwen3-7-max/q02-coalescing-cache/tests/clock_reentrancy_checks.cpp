#include "coalescing_cache.hpp"

#include <cassert>
#include <cstdint>

int main() {
    CoalescingCache* cache_ptr = nullptr;
    std::uint64_t tick = 100;
    CoalescingCache cache(2, [&] {
        assert(cache_ptr != nullptr);
        // Clock invocation must not happen under a cache mutex: invalidate
        // needs that mutex and is a permitted public re-entry.
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
