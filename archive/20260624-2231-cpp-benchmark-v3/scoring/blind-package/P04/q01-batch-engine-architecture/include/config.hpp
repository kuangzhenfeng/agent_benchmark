#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// 引擎配置：由组合根装配，决定线程数、重试上限等运行参数
struct EngineConfig {
    std::size_t worker_count{4};
    std::uint16_t max_attempts{3};
    std::uint32_t backoff_base_ms{50};
    std::uint32_t backoff_cap_ms{2000};
    std::string store_kind{"in-memory"};   // 仅文档性，实际装配由组合根决定
};

// 配置加载器：按运行档位产出默认配置
struct ConfigLoader {
    static EngineConfig default_for(const std::string& profile);
};
