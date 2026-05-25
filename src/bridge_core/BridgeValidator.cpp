// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "BridgeValidator.h"

#include <algorithm>

namespace strategic_nexus::bridge_core {

bool BridgeValidator::hasAnyValidDimension(const BridgePayload& payload) const
{
    return payload.validDimensionCount > 0;
}

ValidatedBridgeState BridgeValidator::validate(
    const BridgePayload& payload,
    const std::int64_t nowUnixMs,
    const std::int64_t lastAcceptedSequenceId) const
{
    if (payload.schemaVersion != 1) {
        return makeFallbackState("unknown schema_version", payload.sequenceId);
    }

    if (payload.sequenceId <= lastAcceptedSequenceId) {
        return makeFallbackState("replayed or old sequence_id", payload.sequenceId);
    }

    if (payload.createdUnixMs < 0 || payload.ttlMs <= 0) {
        return makeFallbackState("invalid timestamp or ttl", payload.sequenceId);
    }

    if (payload.createdUnixMs + payload.ttlMs < nowUnixMs) {
        return makeFallbackState("stale payload", payload.sequenceId);
    }

    if (!payload.bridgeAvailable) {
        return makeFallbackState("bridge unavailable", payload.sequenceId);
    }

    if (payload.campaignId.empty() || payload.empireId.empty()) {
        return makeFallbackState("missing identity fields", payload.sequenceId);
    }

    if (payload.fallbackRequired) {
        return makeFallbackState("fallback required by payload", payload.sequenceId);
    }

    if (!hasAnyValidDimension(payload)) {
        return makeFallbackState("no valid strategy dimensions", payload.sequenceId);
    }

    ValidatedBridgeState state;
    state.valid = true;
    state.reason = "valid";
    state.sequenceId = payload.sequenceId;
    state.bridgeAvailable = true;
    state.campaignId = payload.campaignId;
    state.empireId = payload.empireId;
    state.dimensions = payload.dimensions;
    state.validDimensionCount = payload.validDimensionCount;
    state.invalidDimensionCount = payload.invalidDimensionCount;
    state.confidence = std::clamp(payload.confidence, 0.0, 1.0);
    state.fallbackRequired = false;
    state.action = PredefinedGameAction::ApplyStrategyDimensions;
    return state;
}

} // namespace strategic_nexus::bridge_core

