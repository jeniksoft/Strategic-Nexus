#include "strategic_pipeline/ProcessingPriorityScorer.h"

#include <iostream>
#include <string>

namespace {

using strategic_nexus::strategic_pipeline::MinistryInputContext;
using strategic_nexus::strategic_pipeline::ProcessingPriorityScore;
using strategic_nexus::strategic_pipeline::ProcessingPriorityScorer;

void requireCondition(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        std::exit(1);
    }
}

MinistryInputContext makeInput()
{
    MinistryInputContext input;
    input.campaignId = "campaign_priority";
    input.empireId = "empire_priority";
    input.ministry = "military";
    return input;
}

} // namespace

int main()
{
    const ProcessingPriorityScorer scorer;

    {
        MinistryInputContext input = makeInput();
        input.turnContext.isAtWar = true;
        input.turnContext.strategicPressure = 0.82;
        input.uncertainties.push_back("enemy reserves unknown");
        const ProcessingPriorityScore score = scorer.score(input);
        requireCondition(score.tier == "critical", "active war with high pressure should be critical");
        requireCondition(score.warPriority > 0.0, "active war should contribute priority");
        requireCondition(score.pressurePriority > 0.0, "strategic pressure should contribute priority");
        requireCondition(!score.reasons.empty(), "priority score should preserve audit reasons");
    }

    {
        MinistryInputContext input = makeInput();
        input.turnContext.isAtWar = false;
        input.turnContext.strategicPressure = -5.0;
        const ProcessingPriorityScore score = scorer.score(input);
        requireCondition(score.tier == "low", "quiet empire should remain low priority");
        requireCondition(score.pressurePriority == 0.0, "negative pressure should clamp to zero");
    }

    {
        MinistryInputContext input = makeInput();
        input.turnContext.strategicPressure = 5.0;
        const ProcessingPriorityScore score = scorer.score(input);
        requireCondition(score.pressurePriority == 0.35, "pressure priority should clamp at the v0 cap");
        requireCondition(score.tier == "normal", "high pressure alone should not exceed normal without war/crisis signals");
    }

    std::cout << "v0 priority score tests passed.\n";
    return 0;
}
