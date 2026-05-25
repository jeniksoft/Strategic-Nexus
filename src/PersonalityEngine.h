// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "CoreTypes.h"

#include <string>

namespace strategic_nexus {

class PersonalityEngine {
public:
    std::string describeStrategicBias(const EmpireState& empire) const;
};

} // namespace strategic_nexus

