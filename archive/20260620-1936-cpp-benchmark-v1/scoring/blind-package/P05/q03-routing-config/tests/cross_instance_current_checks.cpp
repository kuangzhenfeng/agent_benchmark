#include "routing_config.hpp"

#include <cassert>

int main() {
    RoutingConfig first(Config{0, {{"/", "first"}}});
    RoutingConfig second(Config{0, {{"/", "second"}}});

    const Config& first_snapshot = first.current();
    assert(first_snapshot.version == 0);
    assert(first_snapshot.routes.front().target == "first");

    // 这一调用不得替换第一个实例的生命周期 pin。
    static_cast<void>(second.current());
    assert(first.reload(Config{0, {{"/", "updated"}}}));

    // AddressSanitizer 会把单一共享 TLS 槽位转化为确定性的
    // use-after-free 失败。正确的实现也应保留旧的不可变快照，
    // 而不是就地修改它。
    assert(first_snapshot.version == 0);
    assert(first_snapshot.routes.front().target == "first");
}
