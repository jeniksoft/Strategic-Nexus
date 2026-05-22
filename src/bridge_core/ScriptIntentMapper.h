#pragma once

#include "BridgeState.h"

#include <string>
#include <vector>

namespace strategic_nexus::bridge_core {

struct ScriptIntent {
    PredefinedGameAction action = PredefinedGameAction::UseFallback;
    std::string eventId = "strategic_nexus.1400";
    std::vector<std::string> globalFlags;
    std::vector<std::string> countryFlags;
    int mappedDimensionCount = 0;
    int unmappedDimensionCount = 0;
};

class ScriptIntentMapper {
public:
    ScriptIntent map(const ValidatedBridgeState& state) const;

private:
    void addBridgeAvailability(ScriptIntent& intent) const;
    void mapMilitaryPosture(const StrategyDimensions& dimensions, ScriptIntent& intent) const;
    void mapResearchBias(const StrategyDimensions& dimensions, ScriptIntent& intent) const;
};

} // namespace strategic_nexus::bridge_core
