#pragma once

#include "CoreTypes.h"

namespace strategic_nexus {

class GalaxyAnalyzer {
public:
    StrategicSummary summarize(const GalaxyState& state) const;
};

} // namespace strategic_nexus
