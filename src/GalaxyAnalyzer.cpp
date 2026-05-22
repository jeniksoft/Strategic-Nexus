#include "GalaxyAnalyzer.h"

#include <algorithm>

namespace strategic_nexus {

StrategicSummary GalaxyAnalyzer::summarize(const GalaxyState& state) const
{
    StrategicSummary summary;
    summary.year = state.year;

    const auto strongest = std::max_element(
        state.empires.begin(),
        state.empires.end(),
        [](const EmpireState& left, const EmpireState& right) {
            return left.power.fleetPower < right.power.fleetPower;
        });

    if (strongest == state.empires.end()) {
        return summary;
    }

    summary.dominantEmpireId = strongest->id;
    summary.dominantFleetPower = strongest->power.fleetPower;

    int totalFleetPower = 0;
    for (const EmpireState& empire : state.empires) {
        totalFleetPower += empire.power.fleetPower;
    }

    const double dominanceRatio = totalFleetPower > 0
        ? static_cast<double>(strongest->power.fleetPower) / static_cast<double>(totalFleetPower)
        : 0.0;

    summary.hegemonyDetected = dominanceRatio >= 0.55;
    summary.instability = std::min(1.0, dominanceRatio + state.history.size() * 0.05);
    return summary;
}

} // namespace strategic_nexus
