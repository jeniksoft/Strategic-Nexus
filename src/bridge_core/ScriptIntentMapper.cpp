#include "ScriptIntentMapper.h"

namespace strategic_nexus::bridge_core {
namespace {

void addGlobalAndCountryFlag(ScriptIntent& intent, const char* flag)
{
    intent.globalFlags.emplace_back(flag);
    intent.countryFlags.emplace_back(flag);
}

} // namespace

ScriptIntent ScriptIntentMapper::map(const ValidatedBridgeState& state) const
{
    ScriptIntent intent;
    intent.action = state.action;
    intent.eventId = "strategic_nexus.1400";

    if (!state.valid || state.action != PredefinedGameAction::ApplyStrategyDimensions) {
        intent.action = PredefinedGameAction::UseFallback;
        return intent;
    }

    addBridgeAvailability(intent);
    mapMilitaryPosture(state.dimensions, intent);
    mapResearchBias(state.dimensions, intent);
    return intent;
}

void ScriptIntentMapper::addBridgeAvailability(ScriptIntent& intent) const
{
    addGlobalAndCountryFlag(intent, "strategic_nexus_bridge_available");
}

void ScriptIntentMapper::mapMilitaryPosture(const StrategyDimensions& dimensions, ScriptIntent& intent) const
{
    if (!dimensions.hasMilitaryPosture) {
        return;
    }

    switch (dimensions.militaryPosture) {
    case MilitaryPosture::Defensive:
        addGlobalAndCountryFlag(intent, "strategic_nexus_military_posture_defensive");
        ++intent.mappedDimensionCount;
        return;
    case MilitaryPosture::Aggressive:
        addGlobalAndCountryFlag(intent, "strategic_nexus_military_posture_aggressive");
        ++intent.mappedDimensionCount;
        return;
    case MilitaryPosture::Passive:
    case MilitaryPosture::Balanced:
    case MilitaryPosture::Existential:
        ++intent.unmappedDimensionCount;
        return;
    }
}

void ScriptIntentMapper::mapResearchBias(const StrategyDimensions& dimensions, ScriptIntent& intent) const
{
    if (!dimensions.hasResearchBias) {
        return;
    }

    switch (dimensions.researchBias) {
    case ResearchBias::Economy:
        addGlobalAndCountryFlag(intent, "strategic_nexus_research_bias_economy");
        ++intent.mappedDimensionCount;
        return;
    case ResearchBias::MilitaryIndustry:
        addGlobalAndCountryFlag(intent, "strategic_nexus_research_bias_military_industry");
        ++intent.mappedDimensionCount;
        return;
    case ResearchBias::Balanced:
    case ResearchBias::Shields:
    case ResearchBias::Armor:
    case ResearchBias::Missiles:
    case ResearchBias::StrikeCraft:
    case ResearchBias::PointDefense:
    case ResearchBias::SensorsIntel:
    case ResearchBias::Megastructure:
    case ResearchBias::CrisisPreparation:
        ++intent.unmappedDimensionCount;
        return;
    }
}

} // namespace strategic_nexus::bridge_core
