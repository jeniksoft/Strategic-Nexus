// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "PersonalityEngine.h"

namespace strategic_nexus {

std::string PersonalityEngine::describeStrategicBias(const EmpireState& empire) const
{
    if (empire.personality.paranoia > 0.65 || empire.adaptiveState.fearOfPlayer > 0.65) {
        return "risk-sensitive balancing";
    }

    if (empire.personality.opportunism > 0.6 && empire.personality.honor < 0.45) {
        return "opportunistic pressure";
    }

    if (empire.personality.boldness > 0.65) {
        return "assertive expansion";
    }

    return "cautious consolidation";
}

} // namespace strategic_nexus

