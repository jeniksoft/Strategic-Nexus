#include "StrategicRequest.h"

#include <algorithm>
#include <cctype>

namespace strategic_nexus {

const char* toString(RequestTrigger trigger)
{
    switch (trigger) {
    case RequestTrigger::Manual:
        return "manual";
    case RequestTrigger::ScheduledDecade:
        return "scheduled_decade";
    case RequestTrigger::WarDeclared:
        return "war_declared";
    case RequestTrigger::FederationFormed:
        return "federation_formed";
    case RequestTrigger::CrisisStageChanged:
        return "crisis_stage_changed";
    case RequestTrigger::EmpireCollapsed:
        return "empire_collapsed";
    case RequestTrigger::HegemonyDetected:
        return "hegemony_detected";
    case RequestTrigger::EndgameStarted:
        return "endgame_started";
    }

    return "unknown";
}

RequestTrigger requestTriggerFromString(const std::string& value)
{
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });

    if (normalized == "scheduled_decade") {
        return RequestTrigger::ScheduledDecade;
    }
    if (normalized == "war_declared") {
        return RequestTrigger::WarDeclared;
    }
    if (normalized == "federation_formed") {
        return RequestTrigger::FederationFormed;
    }
    if (normalized == "crisis_stage_changed") {
        return RequestTrigger::CrisisStageChanged;
    }
    if (normalized == "empire_collapsed") {
        return RequestTrigger::EmpireCollapsed;
    }
    if (normalized == "hegemony_detected") {
        return RequestTrigger::HegemonyDetected;
    }
    if (normalized == "endgame_started") {
        return RequestTrigger::EndgameStarted;
    }

    return RequestTrigger::Manual;
}

} // namespace strategic_nexus
