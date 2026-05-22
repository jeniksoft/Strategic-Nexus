#pragma once

#include <chrono>
#include <filesystem>

namespace strategic_nexus {

struct DaemonConfig {
    std::filesystem::path exchangeDirectory = "exchange";
    std::chrono::milliseconds pollInterval = std::chrono::milliseconds(1000);
    int maxIterations = 0;
};

class DaemonRunner {
public:
    int run(const DaemonConfig& config) const;
};

} // namespace strategic_nexus
