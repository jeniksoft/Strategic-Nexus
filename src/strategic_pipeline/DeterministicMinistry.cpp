#include "DeterministicMinistry.h"

#include <algorithm>

namespace strategic_nexus::strategic_pipeline {
namespace {

double clampConfidence(const double value)
{
    return std::clamp(value, 0.0, 1.0);
}

} // namespace

MinistryProposal DeterministicMinistry::propose(const MinistryInputContext& input) const
{
    MinistryProposal proposal;
    proposal.campaignId = input.campaignId;
    proposal.empireId = input.empireId;
    proposal.ministry = input.ministry;
    proposal.proposalId = input.contextId.empty() ? "proposal_v0" : input.contextId + "_proposal";
    proposal.explicitUncertainties = input.uncertainties;

    const double pressure = std::clamp(input.turnContext.strategicPressure, 0.0, 1.0);

    if (input.ministry == "military") {
        proposal.requestedMilitaryPosture = pressure >= 0.70 ? "aggressive" : "defensive";
        proposal.confidence = clampConfidence(0.55 + pressure * 0.30);
        proposal.statedReasons.push_back(input.turnContext.isAtWar
            ? "Empire is at war, military posture requires review."
            : "Military ministry preserves readiness without tactical control.");
        if (pressure >= 0.70) {
            proposal.statedReasons.push_back("Strategic pressure is high enough to justify aggressive posture.");
        } else {
            proposal.statedReasons.push_back("Strategic pressure favors defensive posture.");
        }
        proposal.explicitRisks.push_back("Deterministic v0 ministry uses limited context only.");
        return proposal;
    }

    if (input.ministry == "research") {
        proposal.requestedResearchBias = pressure >= 0.65 ? "military_industry" : "economy";
        proposal.confidence = clampConfidence(0.55 + pressure * 0.20);
        proposal.statedReasons.push_back("Research ministry uses v0 pressure threshold only.");
        proposal.explicitRisks.push_back("Real technology availability is not parsed in v0.");
        return proposal;
    }

    proposal.confidence = 0.0;
    proposal.explicitRisks.push_back("Unknown ministry; no proposal generated.");
    return proposal;
}

} // namespace strategic_nexus::strategic_pipeline
