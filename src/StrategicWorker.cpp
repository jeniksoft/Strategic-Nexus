// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "StrategicWorker.h"

#include "DoctrinePlanner.h"
#include "GalaxyAnalyzer.h"
#include "LlmClient.h"
#include "MemoryStore.h"
#include "ModIntegration.h"
#include "PersonalityProfileStore.h"
#include "PersonalityEngine.h"
#include "SaveParser.h"

#include <exception>
#include <iostream>

namespace strategic_nexus {

int StrategicWorker::processRequest(const StrategicRequest& request) const
{
    const SaveParser parser;
    const GalaxyState state = parser.parseSnapshot(request.snapshotPath);

    MemoryStore memory;
    memory.ingest(state.history);

    const GalaxyAnalyzer analyzer;
    const StrategicSummary summary = analyzer.summarize(state);

    const PersonalityProfileStore profileStore;
    EmpireState focusEmpire;
    const EmpireState* focusEmpirePtr = nullptr;
    if (!summary.dominantEmpireId.empty()) {
        for (const auto& empire : state.empires) {
            if (empire.id == summary.dominantEmpireId) {
                focusEmpire = empire;
                focusEmpirePtr = &focusEmpire;
                break;
            }
        }
    }

    if (focusEmpirePtr != nullptr && !request.campaignId.empty() && !request.personalityProfileRoot.empty()) {
        const auto loadedProfile = profileStore.load(
            request.personalityProfileRoot,
            request.campaignId,
            focusEmpirePtr->id);
        if (loadedProfile.ok) {
            focusEmpire.personality = loadedProfile.personality;
            focusEmpirePtr = &focusEmpire;
        }
    }

    const DoctrinePlanner planner;
    const DoctrineDecision proposedDecision = planner.chooseDoctrine(summary);

    const PersonalityEngine personalityEngine;
    const DoctrineDecision decision = focusEmpirePtr != nullptr
        ? personalityEngine.refineDoctrineDecision(*focusEmpirePtr, summary, proposedDecision)
        : proposedDecision;
    const std::string personalityAlignmentNote =
        focusEmpirePtr != nullptr
            ? personalityEngine.buildDoctrineAlignmentNote(*focusEmpirePtr, proposedDecision, decision)
            : std::string();
    DoctrineDecision decisionWithAlignmentNote = decision;
    decisionWithAlignmentNote.personalityAlignmentNote = personalityAlignmentNote;

    const LlmClient llmClient;
    const std::string prompt = llmClient.buildPrompt(
        summary,
        decisionWithAlignmentNote,
        focusEmpirePtr != nullptr ? personalityEngine.describeStrategicBias(*focusEmpirePtr) : std::string());

    const ModIntegration modIntegration;
    modIntegration.writeDoctrineJson(request.outputDoctrinePath, request, decisionWithAlignmentNote);

    std::cout << "Request id: " << request.requestId << "\n";
    std::cout << "Strategic request trigger: " << toString(request.trigger) << "\n";
    std::cout << prompt;
    if (!decisionWithAlignmentNote.personalityAlignmentNote.empty()) {
        std::cout << "Personality alignment note: " << decisionWithAlignmentNote.personalityAlignmentNote << "\n";
    }
    std::cout << "Doctrine output: " << request.outputDoctrinePath.string() << "\n";
    std::cout << "Historical events loaded: " << memory.events().size() << "\n";

    return 0;
}

int StrategicWorker::processRequestSafely(const StrategicRequest& request) const
{
    try {
        return processRequest(request);
    }
    catch (const std::exception& error) {
        const ModIntegration modIntegration;
        modIntegration.writeErrorJson(request.outputDoctrinePath, request, error.what());
        std::cerr << "Strategic request failed: " << error.what() << "\n";
        return 1;
    }
}

} // namespace strategic_nexus

