// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "BridgeContract.h"

#include "DoctrinePlanner.h"

#include <iomanip>
#include <sstream>

namespace strategic_nexus {

BridgeValues BridgeContract::buildValues(const DoctrineDecision& decision) const
{
    BridgeValues values;
    values.available = true;
    values.doctrine = toString(decision.type);
    values.strategicPressure = decision.confidence;
    values.fallbackRequired = false;
    return values;
}

std::string BridgeContract::renderJson(const BridgeValues& values) const
{
    std::ostringstream output;
    output << "  \"bridge\": {\n";
    output << "    \"available\": " << (values.available ? "true" : "false") << ",\n";
    output << "    \"doctrine\": \"" << values.doctrine << "\",\n";
    output << "    \"strategic_pressure\": " << std::fixed << std::setprecision(2) << values.strategicPressure << ",\n";
    output << "    \"fallback_required\": " << (values.fallbackRequired ? "true" : "false") << "\n";
    output << "  }";
    return output.str();
}

} // namespace strategic_nexus

