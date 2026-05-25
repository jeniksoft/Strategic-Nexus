// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "BridgeCorePipeline.h"

#include "BridgeValidator.h"
#include "DecisionFileReader.h"
#include "EffectBatchEmitter.h"
#include "EffectBatchFileWriter.h"
#include "ScriptIntentMapper.h"
#include "SequenceStateStore.h"

namespace strategic_nexus::bridge_core {

BridgeCorePipelineResult BridgeCorePipeline::processOnce(const BridgeCorePipelineConfig& config) const
{
    BridgeCorePipelineResult result;

    SequenceStateStore sequenceStore;
    const SequenceReadResult sequenceRead = sequenceStore.read(config.sequenceStatePath);
    if (!sequenceRead.ok) {
        result.reason = sequenceRead.reason;
        return result;
    }

    result.previousSequenceId = sequenceRead.lastAcceptedSequenceId;
    result.currentSequenceId = sequenceRead.lastAcceptedSequenceId;

    DecisionFileReader reader;
    const DecisionReadResult readResult = reader.read(config.decisionPath);
    if (!readResult.ok) {
        EffectBatchFileWriter writer;
        writer.writeAtomically(EffectBatch{}, config.effectBatchPath);
        result.reason = readResult.error;
        return result;
    }

    BridgeValidator validator;
    const ValidatedBridgeState state = validator.validate(
        readResult.payload,
        config.nowUnixMs,
        sequenceRead.lastAcceptedSequenceId);
    result.sequenceId = state.sequenceId;

    ScriptIntentMapper mapper;
    const ScriptIntent intent = mapper.map(state);

    EffectBatchEmitter emitter;
    const EffectBatch batch = emitter.emit(intent);

    EffectBatchFileWriter writer;
    const EffectBatchWriteResult writeResult = writer.writeAtomically(batch, config.effectBatchPath);
    result.effectBatchWritten = writeResult.written;

    if (!state.valid) {
        result.reason = state.reason;
        return result;
    }

    if (!writeResult.written) {
        result.reason = writeResult.reason;
        return result;
    }

    const SequenceWriteResult sequenceWrite = sequenceStore.writeAtomically(
        config.sequenceStatePath,
        state.sequenceId);
    if (!sequenceWrite.written) {
        writer.writeAtomically(EffectBatch{}, config.effectBatchPath);
        result.effectBatchWritten = false;
        result.reason = sequenceWrite.reason;
        return result;
    }

    result.accepted = true;
    result.sequenceAdvanced = true;
    result.currentSequenceId = state.sequenceId;
    result.reason = "accepted";
    return result;
}

} // namespace strategic_nexus::bridge_core

