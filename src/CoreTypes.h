#pragma once

#include <string>
#include <vector>

namespace strategic_nexus {

struct EmpirePower {
    int fleetPower = 0;
    int economicRank = 0;
    int technologyRank = 0;
    int diplomaticInfluence = 0;
};

struct PersonalityProfile {
    double boldness = 0.5;
    double paranoia = 0.5;
    double honor = 0.5;
    double opportunism = 0.5;
};

struct AdaptiveState {
    double fearOfPlayer = 0.0;
    double warTrauma = 0.0;
    double trustInFederations = 0.5;
};

struct HistoricalEvent {
    int year = 0;
    std::string eventType;
    std::string actor;
    std::string target;
    double severity = 0.0;
};

struct EmpireState {
    std::string id;
    std::string displayName;
    EmpirePower power;
    PersonalityProfile personality;
    AdaptiveState adaptiveState;
};

struct GalaxyState {
    int year = 2200;
    std::vector<EmpireState> empires;
    std::vector<HistoricalEvent> history;
};

struct StrategicSummary {
    int year = 2200;
    std::string dominantEmpireId;
    int dominantFleetPower = 0;
    double instability = 0.0;
    bool hegemonyDetected = false;
};

enum class DoctrineType {
    Consolidate,
    BalanceAgainstHegemon,
    OpportunisticExpansion,
    DefensivePosture
};

struct DoctrineDecision {
    DoctrineType type = DoctrineType::Consolidate;
    std::string rationale;
    double confidence = 0.0;
};

} // namespace strategic_nexus
