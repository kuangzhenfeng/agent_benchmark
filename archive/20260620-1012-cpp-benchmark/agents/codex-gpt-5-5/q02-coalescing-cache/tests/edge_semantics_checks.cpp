#include "coalescing_cache.hpp"

#include <atomic>
#include <cassert>

int main() {
    std::atomic<std::uint64_t> now{100};
    CoalescingCache cache(2, [&] { return now.load(); });
    std::atomic<int> loads{0};

    const auto first = cache.get_or_load("a", 10, [&](const std::string&) {
        ++loads;
        const auto recursive = cache.get_or_load("a", 10, [](const std::string&) {
            return LoadResult::ok("must not run");
        });
        assert(recursive.status == LoadStatus::recursive_load);
        return LoadResult::ok("one");
    });
    assert(first.status == LoadStatus::ok && first.value == "one");
    assert(loads == 1);
    assert(cache.get_or_load("a", 10, [&](const std::string&) {
               ++loads;
               return LoadResult::ok("wrong");
           }).value == "one");

    now.store(110);
    assert(cache.get_or_load("a", 10, [&](const std::string&) {
               ++loads;
               return LoadResult::ok("two");
           }).value == "two");
    assert(loads == 2);

    const auto invalidated = cache.get_or_load("b", 10, [&](const std::string&) {
        ++loads;
        cache.invalidate("b");
        return LoadResult::ok("three");
    });
    assert(invalidated.status == LoadStatus::ok);
    assert(cache.size() == 1);  // only key "a" remains cached
    assert(cache.get_or_load("b", 10, [&](const std::string&) {
               ++loads;
               return LoadResult::ok("four");
           }).value == "four");
}
