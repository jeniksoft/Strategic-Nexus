// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "LlmClient.h"

#include "DoctrinePlanner.h"

#include <sstream>

namespace strategic_nexus {

std::string LlmClient::buildPrompt(const StrategicSummary& summary, const DoctrineDecision& decision) const
{
    std::ostringstream prompt;
    prompt << "Strategic Nexus doctrine review\n";
    prompt << "Year: " << summary.year << "\n";
    prompt << "Dominant empire: " << summary.dominantEmpireId << "\n";
    prompt << "Hegemony detected: " << (summary.hegemonyDetected ? "yes" : "no") << "\n";
    prompt << "Selected doctrine: " << toString(decision.type) << "\n";
    prompt << "Rationale: " << decision.rationale << "\n";
    return prompt.str();
}

} // namespace strategic_nexus

