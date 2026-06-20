#include "routing_config.hpp"

#include <cassert>

int main() {
    RoutingConfig first(Config{0, {{"/", "first"}}});
    RoutingConfig second(Config{0, {{"/", "second"}}});

    const Config& first_snapshot = first.current();
    assert(first_snapshot.version == 0);
    assert(first_snapshot.routes.front().target == "first");

    // This must not replace the lifetime pin for the first instance.
    static_cast<void>(second.current());
    assert(first.reload(Config{0, {{"/", "updated"}}}));

    // AddressSanitizer turns a single shared TLS slot into a deterministic
    // use-after-free failure.  A correct implementation also preserves the
    // old immutable snapshot rather than mutating it in place.
    assert(first_snapshot.version == 0);
    assert(first_snapshot.routes.front().target == "first");
}
