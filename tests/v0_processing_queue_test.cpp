#include "strategic_pipeline/EmpireProcessingQueue.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

using strategic_nexus::strategic_pipeline::EmpireProcessingQueue;
using strategic_nexus::strategic_pipeline::EmpireProcessingQueueEntry;
using strategic_nexus::strategic_pipeline::MinistryInputContext;

void requireCondition(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        std::exit(1);
    }
}

MinistryInputContext makeInput(
    const std::string& empireId,
    const bool isAtWar,
    const double strategicPressure,
    std::vector<std::string> uncertainties = {})
{
    MinistryInputContext input;
    input.campaignId = "campaign_queue";
    input.empireId = empireId;
    input.ministry = "military";
    input.turnContext.isAtWar = isAtWar;
    input.turnContext.strategicPressure = strategicPressure;
    input.uncertainties = std::move(uncertainties);
    return input;
}

} // namespace

int main()
{
    const EmpireProcessingQueue queue;
    const std::vector<EmpireProcessingQueueEntry> entries = queue.build({
        makeInput("stable_empire", false, 0.05),
        makeInput("war_empire", true, 0.82, { "enemy reserve strength unknown" }),
        makeInput("pressure_empire", false, 1.00),
        makeInput("tie_empire_a", false, 0.35),
        makeInput("tie_empire_b", false, 0.35)
    });

    requireCondition(entries.size() == 5, "queue should preserve every input");
    requireCondition(entries[0].input.empireId == "war_empire", "active war empire should be processed first");
    requireCondition(entries[1].input.empireId == "pressure_empire", "high pressure peacetime empire should outrank stable empires");
    requireCondition(entries[2].input.empireId == "tie_empire_a", "stable sort should preserve tie order");
    requireCondition(entries[3].input.empireId == "tie_empire_b", "stable sort should preserve tie order");
    requireCondition(entries[4].input.empireId == "stable_empire", "quiet stable empire should be processed last");
    requireCondition(entries[0].priorityScore.totalPriority >= entries[1].priorityScore.totalPriority, "queue should sort descending by priority");

    std::cout << "v0 processing queue tests passed.\n";
    return 0;
}
