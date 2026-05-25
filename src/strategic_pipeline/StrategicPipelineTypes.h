// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus::strategic_pipeline {

struct TurnContext {
    int year = 0;
    bool isAtWar = false;
    double strategicPressure = 0.0;
};

struct ProcessingPriorityScore {
    double basePriority = 0.25;
    double warPriority = 0.0;
    double pressurePriority = 0.0;
    double uncertaintyPriority = 0.0;
    double totalPriority = 0.25;
    std::string tier = "low";
    std::vector<std::string> reasons;
};

struct MinistryInputContext {
    int schemaVersion = 0;
    std::string contextId;
    std::string campaignId;
    std::string empireId;
    std::string ministry;
    TurnContext turnContext;
    std::vector<std::string> knownFacts;
    std::vector<std::string> uncertainties;
    std::string currentMilitaryPosture;
    std::string currentResearchBias;
};

struct MinistryProposal {
    int schemaVersion = 1;
    std::string campaignId;
    std::string empireId;
    std::string ministry;
    std::string proposalId;
    std::string requestedMilitaryPosture;
    std::string requestedResearchBias;
    std::vector<std::string> statedReasons;
    std::vector<std::string> explicitRisks;
    std::vector<std::string> explicitUncertainties;
    double confidence = 0.0;
};

struct ClerkInputBrief {
    int schemaVersion = 1;
    std::string campaignId;
    std::string empireId;
    std::string ministry;
    std::string sourceContextId;
    std::vector<std::string> relevantFacts;
    std::vector<std::string> explicitUncertainties;
    std::vector<std::string> missingInformation;
    std::vector<std::string> compressionNotes;
};

struct FinalStrategyPayload {
    int schemaVersion = 1;
    std::int64_t sequenceId = 0;
    std::int64_t createdUnixMs = 0;
    std::int64_t ttlMs = 30000;
    std::string campaignId;
    std::string empireId;
    std::string militaryPosture;
    std::string researchBias;
    double confidence = 0.0;
    bool fallbackRequired = false;
};

struct PipelineRunConfig {
    std::filesystem::path inputContextPath;
    std::filesystem::path decisionOutputPath;
    std::filesystem::path auditOutputPath;
    std::int64_t sequenceId = 1;
    std::int64_t createdUnixMs = 0;
    std::int64_t ttlMs = 30000;
};

struct PipelineRunResult {
    bool success = false;
    bool decisionOutputWritten = false;
    bool auditOutputRequested = false;
    bool auditOutputWritten = false;
    std::string reason;
    std::string auditReason;
    MinistryInputContext input;
    ProcessingPriorityScore priorityScore;
    MinistryProposal proposal;
    ClerkInputBrief brief;
    FinalStrategyPayload payload;
};

} // namespace strategic_nexus::strategic_pipeline

