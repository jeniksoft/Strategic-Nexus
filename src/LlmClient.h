// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "CoreTypes.h"

#include <string>

namespace strategic_nexus {

class LlmClient {
public:
    std::string buildPrompt(
        const StrategicSummary& summary,
        const DoctrineDecision& decision,
        const std::string& personalityBias = {}) const;
};

} // namespace strategic_nexus

