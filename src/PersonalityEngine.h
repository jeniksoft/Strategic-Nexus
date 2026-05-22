#pragma once

#include "CoreTypes.h"

#include <string>

namespace strategic_nexus {

class PersonalityEngine {
public:
    std::string describeStrategicBias(const EmpireState& empire) const;
};

} // namespace strategic_nexus
