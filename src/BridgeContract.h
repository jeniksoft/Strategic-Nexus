#pragma once

#include "CoreTypes.h"

#include <string>

namespace strategic_nexus {

struct BridgeValues {
    bool available = false;
    std::string doctrine = "fallback_scripted";
    double strategicPressure = 0.0;
    bool fallbackRequired = true;
};

class BridgeContract {
public:
    BridgeValues buildValues(const DoctrineDecision& decision) const;
    std::string renderJson(const BridgeValues& values) const;
};

} // namespace strategic_nexus
