#pragma once

#include "BridgeState.h"

#include <cstdint>

namespace strategic_nexus::bridge_core {

class BridgeValidator {
public:
    ValidatedBridgeState validate(
        const BridgePayload& payload,
        std::int64_t nowUnixMs,
        std::int64_t lastAcceptedSequenceId) const;

private:
    bool hasAnyValidDimension(const BridgePayload& payload) const;
};

} // namespace strategic_nexus::bridge_core
