// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "CoreTypes.h"

namespace strategic_nexus {

class GalaxyAnalyzer {
public:
    StrategicSummary summarize(const GalaxyState& state) const;
};

} // namespace strategic_nexus

