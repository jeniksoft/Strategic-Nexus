// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "StrategicWorker.h"

#include "DoctrinePlanner.h"
#include "GalaxyAnalyzer.h"
#include "LlmClient.h"
#include "MemoryStore.h"
#include "ModIntegration.h"
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

    const DoctrinePlanner planner;
    const DoctrineDecision decision = planner.chooseDoctrine(summary);

    const LlmClient llmClient;
    const std::string prompt = llmClient.buildPrompt(summary, decision);

    const ModIntegration modIntegration;
    modIntegration.writeDoctrineJson(request.outputDoctrinePath, request, decision);

    std::cout << "Request id: " << request.requestId << "\n";
    std::cout << "Strategic request trigger: " << toString(request.trigger) << "\n";
    std::cout << prompt;
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

