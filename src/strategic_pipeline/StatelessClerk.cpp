// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "StatelessClerk.h"

namespace strategic_nexus::strategic_pipeline {

ClerkInputBrief StatelessClerk::reduce(const MinistryInputContext& input) const
{
    ClerkInputBrief brief;
    brief.campaignId = input.campaignId;
    brief.empireId = input.empireId;
    brief.ministry = input.ministry;
    brief.sourceContextId = input.contextId;
    brief.relevantFacts = input.knownFacts;
    brief.explicitUncertainties = input.uncertainties;

    if (input.knownFacts.empty()) {
        brief.missingInformation.push_back("known_facts not specified in ministry input.");
    }
    if (input.uncertainties.empty()) {
        brief.missingInformation.push_back("uncertainties not specified in ministry input.");
    }
    if (input.currentMilitaryPosture.empty()) {
        brief.missingInformation.push_back("current military_posture not specified in current_agenda.");
    }
    if (input.currentResearchBias.empty()) {
        brief.missingInformation.push_back("current research_bias not specified in current_agenda.");
    }

    brief.compressionNotes.push_back("Deterministic v0 clerk copied bounded input fields without external context.");
    return brief;
}

} // namespace strategic_nexus::strategic_pipeline

