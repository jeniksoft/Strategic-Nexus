// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "CoreTypes.h"

#include <string>

namespace strategic_nexus {

class PersonalityEngine {
public:
    std::string describeStrategicBias(const EmpireState& empire) const;
    std::string buildDoctrineAlignmentNote(
        const EmpireState& empire,
        const DoctrineDecision& proposedDecision,
        const DoctrineDecision& refinedDecision) const;
    DoctrineDecision refineDoctrineDecision(
        const EmpireState& empire,
        const StrategicSummary& summary,
        const DoctrineDecision& proposedDecision) const;
};

} // namespace strategic_nexus

