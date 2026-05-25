// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "DoctrinePlanner.h"

namespace strategic_nexus {

DoctrineDecision DoctrinePlanner::chooseDoctrine(const StrategicSummary& summary) const
{
    if (summary.hegemonyDetected) {
        return {
            DoctrineType::BalanceAgainstHegemon,
            "Dominant fleet share indicates a hegemonic pressure point.",
            0.74
        };
    }

    if (summary.instability > 0.65) {
        return {
            DoctrineType::DefensivePosture,
            "Instability is high enough to favor defensive commitments.",
            0.62
        };
    }

    return {
        DoctrineType::Consolidate,
        "No immediate hegemonic or crisis pressure detected.",
        0.58
    };
}

const char* toString(DoctrineType type)
{
    switch (type) {
    case DoctrineType::Consolidate:
        return "consolidate";
    case DoctrineType::BalanceAgainstHegemon:
        return "balance_against_hegemon";
    case DoctrineType::OpportunisticExpansion:
        return "opportunistic_expansion";
    case DoctrineType::DefensivePosture:
        return "defensive_posture";
    }

    return "unknown";
}

} // namespace strategic_nexus

