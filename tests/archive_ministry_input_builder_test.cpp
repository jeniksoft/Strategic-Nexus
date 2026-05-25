// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "ArchiveMinistryInputBuilder.h"

#include <iostream>
#include <string>

namespace {

void requireCondition(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        std::exit(1);
    }
}

} // namespace

int main()
{
    strategic_nexus::AutosaveArchiveSummary summary;
    summary.ok = true;
    summary.copiedSaveCount = 2;
    summary.totalByteCount = 42;
    summary.firstContentHash = "first_hash";
    summary.lastContentHash = "last_hash";

    const strategic_nexus::ArchiveMinistryInputBuilder builder;
    const auto input = builder.build(summary, "campaign_001", "empire_001", "military");

    requireCondition(input.schemaVersion == 1, "builder should emit schema v1");
    requireCondition(input.campaignId == "campaign_001", "builder should preserve campaign id");
    requireCondition(input.empireId == "empire_001", "builder should preserve empire id");
    requireCondition(input.ministry == "military", "builder should preserve ministry");
    requireCondition(input.knownFacts.size() == 4, "builder should emit bounded archive facts");
    requireCondition(input.uncertainties.size() == 2, "builder should emit explicit archive uncertainties");
    requireCondition(input.currentMilitaryPosture == "defensive", "builder should emit safe military fallback");
    requireCondition(input.currentResearchBias == "economy", "builder should emit safe research fallback");

    std::cout << "archive ministry input builder tests passed.\n";
    return 0;
}

