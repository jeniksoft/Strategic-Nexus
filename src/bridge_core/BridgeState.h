// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <cstdint>
#include <string>

namespace strategic_nexus::bridge_core {

enum class PredefinedGameAction {
    UseFallback = 0,
    ApplyStrategyDimensions
};

enum class MilitaryPosture {
    Passive,
    Defensive,
    Balanced,
    Aggressive,
    Existential
};

enum class FleetPhilosophy {
    Balanced,
    Artillery,
    Carrier,
    Missile,
    Torpedo,
    Picket,
    Swarm,
    CrisisDefense
};

enum class BorderStrategy {
    OpenBorder,
    MobileResponse,
    FortressBorder,
    ChokepointHardening,
    DeepSpaceProjection
};

enum class ExpansionPolicy {
    Isolationist,
    Cautious,
    Opportunistic,
    Expansionist,
    ConquestDriven
};

enum class DiplomaticBehavior {
    Cooperative,
    Cautious,
    Manipulative,
    Paranoid,
    HegemonSeeking,
    Vassalizing,
    HonorBound,
    Treacherous
};

enum class ResearchBias {
    Balanced,
    Economy,
    MilitaryIndustry,
    Shields,
    Armor,
    Missiles,
    StrikeCraft,
    PointDefense,
    SensorsIntel,
    Megastructure,
    CrisisPreparation
};

enum class EconomicFocus {
    Balanced,
    Recovery,
    AlloyFocus,
    ResearchFocus,
    UnityFocus,
    NavalCapacity,
    MegastructureFocus,
    FortressEconomy
};

enum class CrisisPolicy {
    Ignore,
    Observe,
    Prepare,
    Contain,
    TotalMobilization,
    SurvivalAtAllCosts
};

enum class EspionagePolicy {
    Ignore,
    DefensiveCounterintel,
    TargetRivals,
    TargetHegemon,
    GatherMilitaryIntel,
    DestabilizeThreats
};

struct StrategyDimensions {
    bool hasMilitaryPosture = false;
    MilitaryPosture militaryPosture = MilitaryPosture::Balanced;

    bool hasFleetPhilosophy = false;
    FleetPhilosophy fleetPhilosophy = FleetPhilosophy::Balanced;

    bool hasBorderStrategy = false;
    BorderStrategy borderStrategy = BorderStrategy::MobileResponse;

    bool hasExpansionPolicy = false;
    ExpansionPolicy expansionPolicy = ExpansionPolicy::Cautious;

    bool hasDiplomaticBehavior = false;
    DiplomaticBehavior diplomaticBehavior = DiplomaticBehavior::Cautious;

    bool hasResearchBias = false;
    ResearchBias researchBias = ResearchBias::Balanced;

    bool hasEconomicFocus = false;
    EconomicFocus economicFocus = EconomicFocus::Balanced;

    bool hasCrisisPolicy = false;
    CrisisPolicy crisisPolicy = CrisisPolicy::Observe;

    bool hasEspionagePolicy = false;
    EspionagePolicy espionagePolicy = EspionagePolicy::Ignore;
};

struct BridgePayload {
    int schemaVersion = 0;
    std::int64_t sequenceId = 0;
    std::int64_t createdUnixMs = 0;
    std::int64_t ttlMs = 0;
    bool bridgeAvailable = false;
    std::string campaignId;
    std::string empireId;
    StrategyDimensions dimensions;
    int validDimensionCount = 0;
    int invalidDimensionCount = 0;
    double confidence = 0.0;
    bool fallbackRequired = true;
};

struct ValidatedBridgeState {
    bool valid = false;
    std::string reason = "unvalidated";
    std::int64_t sequenceId = 0;
    bool bridgeAvailable = false;
    std::string campaignId;
    std::string empireId;
    StrategyDimensions dimensions;
    int validDimensionCount = 0;
    int invalidDimensionCount = 0;
    double confidence = 0.0;
    bool fallbackRequired = true;
    PredefinedGameAction action = PredefinedGameAction::UseFallback;
};

const char* toString(PredefinedGameAction action);
const char* toString(MilitaryPosture value);
const char* toString(FleetPhilosophy value);
const char* toString(BorderStrategy value);
const char* toString(ExpansionPolicy value);
const char* toString(DiplomaticBehavior value);
const char* toString(ResearchBias value);
const char* toString(EconomicFocus value);
const char* toString(CrisisPolicy value);
const char* toString(EspionagePolicy value);
bool tryParseMilitaryPosture(const std::string& value, MilitaryPosture& output);
bool tryParseFleetPhilosophy(const std::string& value, FleetPhilosophy& output);
bool tryParseBorderStrategy(const std::string& value, BorderStrategy& output);
bool tryParseExpansionPolicy(const std::string& value, ExpansionPolicy& output);
bool tryParseDiplomaticBehavior(const std::string& value, DiplomaticBehavior& output);
bool tryParseResearchBias(const std::string& value, ResearchBias& output);
bool tryParseEconomicFocus(const std::string& value, EconomicFocus& output);
bool tryParseCrisisPolicy(const std::string& value, CrisisPolicy& output);
bool tryParseEspionagePolicy(const std::string& value, EspionagePolicy& output);
ValidatedBridgeState makeFallbackState(std::string reason, std::int64_t sequenceId = 0);

} // namespace strategic_nexus::bridge_core

