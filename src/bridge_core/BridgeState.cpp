// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "BridgeState.h"

#include <utility>

namespace strategic_nexus::bridge_core {

const char* toString(const PredefinedGameAction action)
{
    switch (action) {
    case PredefinedGameAction::UseFallback:
        return "UseFallback";
    case PredefinedGameAction::ApplyStrategyDimensions:
        return "ApplyStrategyDimensions";
    }

    return "UnknownAction";
}

const char* toString(const MilitaryPosture value)
{
    switch (value) {
    case MilitaryPosture::Passive: return "passive";
    case MilitaryPosture::Defensive: return "defensive";
    case MilitaryPosture::Balanced: return "balanced";
    case MilitaryPosture::Aggressive: return "aggressive";
    case MilitaryPosture::Existential: return "existential";
    }
    return "unknown";
}

const char* toString(const FleetPhilosophy value)
{
    switch (value) {
    case FleetPhilosophy::Balanced: return "balanced";
    case FleetPhilosophy::Artillery: return "artillery";
    case FleetPhilosophy::Carrier: return "carrier";
    case FleetPhilosophy::Missile: return "missile";
    case FleetPhilosophy::Torpedo: return "torpedo";
    case FleetPhilosophy::Picket: return "picket";
    case FleetPhilosophy::Swarm: return "swarm";
    case FleetPhilosophy::CrisisDefense: return "crisis_defense";
    }
    return "unknown";
}

const char* toString(const BorderStrategy value)
{
    switch (value) {
    case BorderStrategy::OpenBorder: return "open_border";
    case BorderStrategy::MobileResponse: return "mobile_response";
    case BorderStrategy::FortressBorder: return "fortress_border";
    case BorderStrategy::ChokepointHardening: return "chokepoint_hardening";
    case BorderStrategy::DeepSpaceProjection: return "deep_space_projection";
    }
    return "unknown";
}

const char* toString(const ExpansionPolicy value)
{
    switch (value) {
    case ExpansionPolicy::Isolationist: return "isolationist";
    case ExpansionPolicy::Cautious: return "cautious";
    case ExpansionPolicy::Opportunistic: return "opportunistic";
    case ExpansionPolicy::Expansionist: return "expansionist";
    case ExpansionPolicy::ConquestDriven: return "conquest_driven";
    }
    return "unknown";
}

const char* toString(const DiplomaticBehavior value)
{
    switch (value) {
    case DiplomaticBehavior::Cooperative: return "cooperative";
    case DiplomaticBehavior::Cautious: return "cautious";
    case DiplomaticBehavior::Manipulative: return "manipulative";
    case DiplomaticBehavior::Paranoid: return "paranoid";
    case DiplomaticBehavior::HegemonSeeking: return "hegemon_seeking";
    case DiplomaticBehavior::Vassalizing: return "vassalizing";
    case DiplomaticBehavior::HonorBound: return "honor_bound";
    case DiplomaticBehavior::Treacherous: return "treacherous";
    }
    return "unknown";
}

const char* toString(const ResearchBias value)
{
    switch (value) {
    case ResearchBias::Balanced: return "balanced";
    case ResearchBias::Economy: return "economy";
    case ResearchBias::MilitaryIndustry: return "military_industry";
    case ResearchBias::Shields: return "shields";
    case ResearchBias::Armor: return "armor";
    case ResearchBias::Missiles: return "missiles";
    case ResearchBias::StrikeCraft: return "strike_craft";
    case ResearchBias::PointDefense: return "point_defense";
    case ResearchBias::SensorsIntel: return "sensors_intel";
    case ResearchBias::Megastructure: return "megastructure";
    case ResearchBias::CrisisPreparation: return "crisis_preparation";
    }
    return "unknown";
}

const char* toString(const EconomicFocus value)
{
    switch (value) {
    case EconomicFocus::Balanced: return "balanced";
    case EconomicFocus::Recovery: return "recovery";
    case EconomicFocus::AlloyFocus: return "alloy_focus";
    case EconomicFocus::ResearchFocus: return "research_focus";
    case EconomicFocus::UnityFocus: return "unity_focus";
    case EconomicFocus::NavalCapacity: return "naval_capacity";
    case EconomicFocus::MegastructureFocus: return "megastructure_focus";
    case EconomicFocus::FortressEconomy: return "fortress_economy";
    }
    return "unknown";
}

const char* toString(const CrisisPolicy value)
{
    switch (value) {
    case CrisisPolicy::Ignore: return "ignore";
    case CrisisPolicy::Observe: return "observe";
    case CrisisPolicy::Prepare: return "prepare";
    case CrisisPolicy::Contain: return "contain";
    case CrisisPolicy::TotalMobilization: return "total_mobilization";
    case CrisisPolicy::SurvivalAtAllCosts: return "survival_at_all_costs";
    }
    return "unknown";
}

const char* toString(const EspionagePolicy value)
{
    switch (value) {
    case EspionagePolicy::Ignore: return "ignore";
    case EspionagePolicy::DefensiveCounterintel: return "defensive_counterintel";
    case EspionagePolicy::TargetRivals: return "target_rivals";
    case EspionagePolicy::TargetHegemon: return "target_hegemon";
    case EspionagePolicy::GatherMilitaryIntel: return "gather_military_intel";
    case EspionagePolicy::DestabilizeThreats: return "destabilize_threats";
    }
    return "unknown";
}

bool tryParseMilitaryPosture(const std::string& value, MilitaryPosture& output)
{
    if (value == "passive") { output = MilitaryPosture::Passive; return true; }
    if (value == "defensive") { output = MilitaryPosture::Defensive; return true; }
    if (value == "balanced") { output = MilitaryPosture::Balanced; return true; }
    if (value == "aggressive") { output = MilitaryPosture::Aggressive; return true; }
    if (value == "existential") { output = MilitaryPosture::Existential; return true; }
    return false;
}

bool tryParseFleetPhilosophy(const std::string& value, FleetPhilosophy& output)
{
    if (value == "balanced") { output = FleetPhilosophy::Balanced; return true; }
    if (value == "artillery") { output = FleetPhilosophy::Artillery; return true; }
    if (value == "carrier") { output = FleetPhilosophy::Carrier; return true; }
    if (value == "missile") { output = FleetPhilosophy::Missile; return true; }
    if (value == "torpedo") { output = FleetPhilosophy::Torpedo; return true; }
    if (value == "picket") { output = FleetPhilosophy::Picket; return true; }
    if (value == "swarm") { output = FleetPhilosophy::Swarm; return true; }
    if (value == "crisis_defense") { output = FleetPhilosophy::CrisisDefense; return true; }
    return false;
}

bool tryParseBorderStrategy(const std::string& value, BorderStrategy& output)
{
    if (value == "open_border") { output = BorderStrategy::OpenBorder; return true; }
    if (value == "mobile_response") { output = BorderStrategy::MobileResponse; return true; }
    if (value == "fortress_border") { output = BorderStrategy::FortressBorder; return true; }
    if (value == "chokepoint_hardening") { output = BorderStrategy::ChokepointHardening; return true; }
    if (value == "deep_space_projection") { output = BorderStrategy::DeepSpaceProjection; return true; }
    return false;
}

bool tryParseExpansionPolicy(const std::string& value, ExpansionPolicy& output)
{
    if (value == "isolationist") { output = ExpansionPolicy::Isolationist; return true; }
    if (value == "cautious") { output = ExpansionPolicy::Cautious; return true; }
    if (value == "opportunistic") { output = ExpansionPolicy::Opportunistic; return true; }
    if (value == "expansionist") { output = ExpansionPolicy::Expansionist; return true; }
    if (value == "conquest_driven") { output = ExpansionPolicy::ConquestDriven; return true; }
    return false;
}

bool tryParseDiplomaticBehavior(const std::string& value, DiplomaticBehavior& output)
{
    if (value == "cooperative") { output = DiplomaticBehavior::Cooperative; return true; }
    if (value == "cautious") { output = DiplomaticBehavior::Cautious; return true; }
    if (value == "manipulative") { output = DiplomaticBehavior::Manipulative; return true; }
    if (value == "paranoid") { output = DiplomaticBehavior::Paranoid; return true; }
    if (value == "hegemon_seeking") { output = DiplomaticBehavior::HegemonSeeking; return true; }
    if (value == "vassalizing") { output = DiplomaticBehavior::Vassalizing; return true; }
    if (value == "honor_bound") { output = DiplomaticBehavior::HonorBound; return true; }
    if (value == "treacherous") { output = DiplomaticBehavior::Treacherous; return true; }
    return false;
}

bool tryParseResearchBias(const std::string& value, ResearchBias& output)
{
    if (value == "balanced") { output = ResearchBias::Balanced; return true; }
    if (value == "economy") { output = ResearchBias::Economy; return true; }
    if (value == "military_industry") { output = ResearchBias::MilitaryIndustry; return true; }
    if (value == "shields") { output = ResearchBias::Shields; return true; }
    if (value == "armor") { output = ResearchBias::Armor; return true; }
    if (value == "missiles") { output = ResearchBias::Missiles; return true; }
    if (value == "strike_craft") { output = ResearchBias::StrikeCraft; return true; }
    if (value == "point_defense") { output = ResearchBias::PointDefense; return true; }
    if (value == "sensors_intel") { output = ResearchBias::SensorsIntel; return true; }
    if (value == "megastructure") { output = ResearchBias::Megastructure; return true; }
    if (value == "crisis_preparation") { output = ResearchBias::CrisisPreparation; return true; }
    return false;
}

bool tryParseEconomicFocus(const std::string& value, EconomicFocus& output)
{
    if (value == "balanced") { output = EconomicFocus::Balanced; return true; }
    if (value == "recovery") { output = EconomicFocus::Recovery; return true; }
    if (value == "alloy_focus") { output = EconomicFocus::AlloyFocus; return true; }
    if (value == "research_focus") { output = EconomicFocus::ResearchFocus; return true; }
    if (value == "unity_focus") { output = EconomicFocus::UnityFocus; return true; }
    if (value == "naval_capacity") { output = EconomicFocus::NavalCapacity; return true; }
    if (value == "megastructure_focus") { output = EconomicFocus::MegastructureFocus; return true; }
    if (value == "fortress_economy") { output = EconomicFocus::FortressEconomy; return true; }
    return false;
}

bool tryParseCrisisPolicy(const std::string& value, CrisisPolicy& output)
{
    if (value == "ignore") { output = CrisisPolicy::Ignore; return true; }
    if (value == "observe") { output = CrisisPolicy::Observe; return true; }
    if (value == "prepare") { output = CrisisPolicy::Prepare; return true; }
    if (value == "contain") { output = CrisisPolicy::Contain; return true; }
    if (value == "total_mobilization") { output = CrisisPolicy::TotalMobilization; return true; }
    if (value == "survival_at_all_costs") { output = CrisisPolicy::SurvivalAtAllCosts; return true; }
    return false;
}

bool tryParseEspionagePolicy(const std::string& value, EspionagePolicy& output)
{
    if (value == "ignore") { output = EspionagePolicy::Ignore; return true; }
    if (value == "defensive_counterintel") { output = EspionagePolicy::DefensiveCounterintel; return true; }
    if (value == "target_rivals") { output = EspionagePolicy::TargetRivals; return true; }
    if (value == "target_hegemon") { output = EspionagePolicy::TargetHegemon; return true; }
    if (value == "gather_military_intel") { output = EspionagePolicy::GatherMilitaryIntel; return true; }
    if (value == "destabilize_threats") { output = EspionagePolicy::DestabilizeThreats; return true; }
    return false;
}

ValidatedBridgeState makeFallbackState(std::string reason, const std::int64_t sequenceId)
{
    ValidatedBridgeState state;
    state.valid = false;
    state.reason = std::move(reason);
    state.sequenceId = sequenceId;
    state.bridgeAvailable = false;
    state.validDimensionCount = 0;
    state.invalidDimensionCount = 0;
    state.confidence = 0.0;
    state.fallbackRequired = true;
    state.action = PredefinedGameAction::UseFallback;
    return state;
}

} // namespace strategic_nexus::bridge_core

