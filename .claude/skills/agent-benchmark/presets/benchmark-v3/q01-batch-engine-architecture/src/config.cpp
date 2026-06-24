#include "config.hpp"

EngineConfig ConfigLoader::default_for(const std::string& profile) {
    EngineConfig cfg;
    if (profile == "fast") {
        cfg.worker_count = 2;
        cfg.max_attempts = 1;
    } else if (profile == "robust") {
        cfg.worker_count = 8;
        cfg.max_attempts = 5;
        cfg.backoff_cap_ms = 5000;
    } else {
        // 默认档位
        cfg.worker_count = 4;
        cfg.max_attempts = 3;
    }
    return cfg;
}
