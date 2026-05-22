#include "ProcessingPriorityScorer.h"

#include <algorithm>

namespace strategic_nexus::strategic_pipeline {
namespace {

std::string tierForPriority(const double totalPriority)
{
    if (totalPriority >= 0.95) {
        return "critical";
    }
    if (totalPriority >= 0.75) {
        return "high";
    }
    if (totalPriority >= 0.35) {
        return "normal";
    }
    return "low";
}

} // namespace

ProcessingPriorityScore ProcessingPriorityScorer::score(const MinistryInputContext& input) const
{
    ProcessingPriorityScore score;

    const double pressure = std::clamp(input.turnContext.strategicPressure, 0.0, 1.0);
    score.pressurePriority = pressure * 0.35;

    if (input.turnContext.isAtWar) {
        score.warPriority = 0.45;
        score.reasons.push_back("active war increases processing priority.");
    }

    if (pressure >= 0.70) {
        score.reasons.push_back("high strategic pressure increases processing priority.");
    } else if (pressure >= 0.35) {
        score.reasons.push_back("moderate strategic pressure keeps normal processing priority.");
    }

    if (!input.uncertainties.empty()) {
        const double boundedUncertaintyCount = std::min<double>(input.uncertainties.size(), 8.0);
        score.uncertaintyPriority = (boundedUncertaintyCount / 8.0) * 0.10;
        score.reasons.push_back("explicit uncertainty slightly increases review priority.");
    }

    score.totalPriority = score.basePriority
        + score.warPriority
        + score.pressurePriority
        + score.uncertaintyPriority;
    score.tier = tierForPriority(score.totalPriority);

    if (score.reasons.empty()) {
        score.reasons.push_back("no escalation triggers; low priority background review.");
    }

    return score;
}

} // namespace strategic_nexus::strategic_pipeline
