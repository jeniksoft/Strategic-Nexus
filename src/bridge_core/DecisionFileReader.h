#pragma once

#include "BridgeState.h"

#include <filesystem>
#include <string>

namespace strategic_nexus::bridge_core {

struct DecisionReadResult {
    bool ok = false;
    BridgePayload payload;
    std::string error;
};

class DecisionFileReader {
public:
    DecisionReadResult read(const std::filesystem::path& path) const;
};

} // namespace strategic_nexus::bridge_core
