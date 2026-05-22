#include "EffectBatchEmitter.h"

namespace strategic_nexus::bridge_core {
namespace {

std::string makeGlobalFlagLine(const std::string& flag)
{
    return "effect set_global_flag = " + flag;
}

std::string makeCountryFlagLine(const std::string& flag)
{
    return "effect set_country_flag = " + flag;
}

std::string makeEventLine(const std::string& eventId)
{
    return "event " + eventId;
}

} // namespace

EffectBatch EffectBatchEmitter::emit(const ScriptIntent& intent) const
{
    EffectBatch batch;

    if (intent.action != PredefinedGameAction::ApplyStrategyDimensions || intent.mappedDimensionCount <= 0) {
        return batch;
    }

    for (const std::string& flag : intent.globalFlags) {
        batch.lines.push_back(makeGlobalFlagLine(flag));
    }

    for (const std::string& flag : intent.countryFlags) {
        batch.lines.push_back(makeCountryFlagLine(flag));
    }

    batch.lines.push_back(makeEventLine(intent.eventId));
    batch.valid = true;
    return batch;
}

} // namespace strategic_nexus::bridge_core
