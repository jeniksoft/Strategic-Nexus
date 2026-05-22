#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace strategic_nexus::bridge_core {

struct BridgeCorePipelineConfig {
    std::filesystem::path decisionPath;
    std::filesystem::path effectBatchPath;
    std::filesystem::path sequenceStatePath;
    std::int64_t nowUnixMs = 0;
};

struct BridgeCorePipelineResult {
    bool accepted = false;
    bool effectBatchWritten = false;
    bool sequenceAdvanced = false;
    std::string reason;
    std::int64_t sequenceId = 0;
    std::int64_t previousSequenceId = 0;
    std::int64_t currentSequenceId = 0;
};

class BridgeCorePipeline {
public:
    BridgeCorePipelineResult processOnce(const BridgeCorePipelineConfig& config) const;
};

} // namespace strategic_nexus::bridge_core
