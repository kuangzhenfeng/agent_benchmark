#include "metrics.hpp"

std::string MetricsCollector::summary() const {
    std::string out;
    out += "submitted=";
    out += std::to_string(submitted());
    out += " succeeded=";
    out += std::to_string(succeeded());
    out += " failed=";
    out += std::to_string(failed());
    out += " retried=";
    out += std::to_string(retried());
    return out;
}
