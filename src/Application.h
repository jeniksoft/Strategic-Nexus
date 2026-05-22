#pragma once

#include "StrategicRequest.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace strategic_nexus {

struct RunConfig {
    bool daemonMode = false;
    bool bridgePipelineMode = false;
    bool v0PipelineMode = false;
    bool v0PriorityQueueMode = false;
    std::filesystem::path exchangeDirectory = "exchange";
    std::chrono::milliseconds daemonPollInterval = std::chrono::milliseconds(1000);
    int daemonMaxIterations = 0;
    std::filesystem::path bridgeDecisionPath;
    std::filesystem::path bridgeEffectBatchPath;
    std::filesystem::path bridgeSequenceStatePath;
    std::int64_t bridgeNowUnixMs = 0;
    std::filesystem::path v0InputContextPath;
    std::filesystem::path v0DecisionOutputPath;
    std::filesystem::path v0AuditOutputPath;
    std::filesystem::path v0PriorityQueueOutputPath;
    std::vector<std::filesystem::path> v0PriorityQueueInputPaths;
    std::int64_t v0SequenceId = 1;
    std::int64_t v0CreatedUnixMs = 0;
    std::int64_t v0TtlMs = 30000;
    StrategicRequest request;
};

class Application {
public:
    int run(const RunConfig& config) const;
};

RunConfig parseRunConfig(int argc, char* argv[]);

} // namespace strategic_nexus
