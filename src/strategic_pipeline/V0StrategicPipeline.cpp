#include "V0StrategicPipeline.h"

#include "DeterministicMinistry.h"
#include "LightweightCabinet.h"
#include "MinistryInputReader.h"
#include "PipelineJsonWriter.h"
#include "ProcessingPriorityScorer.h"
#include "StatelessClerk.h"

#include "common/FileUtil.h"

namespace strategic_nexus::strategic_pipeline {

PipelineRunResult V0StrategicPipeline::run(const PipelineRunConfig& config) const
{
    PipelineRunResult result;

    MinistryInputReader reader;
    const MinistryInputReadResult readResult = reader.read(config.inputContextPath);
    if (!readResult.ok) {
        result.reason = readResult.error;
        return result;
    }

    result.input = readResult.input;

    ProcessingPriorityScorer priorityScorer;
    result.priorityScore = priorityScorer.score(result.input);

    DeterministicMinistry ministry;
    result.proposal = ministry.propose(result.input);

    StatelessClerk clerk;
    result.brief = clerk.reduce(result.input);

    LightweightCabinet cabinet;
    result.payload = cabinet.evaluate(
        result.input,
        result.proposal,
        result.brief,
        config.sequenceId,
        config.createdUnixMs,
        config.ttlMs);

    if (!common::writeTextFileAtomically(config.decisionOutputPath, serializeFinalStrategyPayload(result.payload))) {
        result.payload.fallbackRequired = true;
        result.reason = "failed to write decision output";
        return result;
    }
    result.decisionOutputWritten = true;

    result.success = !result.payload.fallbackRequired;
    result.reason = result.success ? "accepted" : "fallback required";
    if (!config.auditOutputPath.empty()) {
        result.auditOutputRequested = true;
        if (!common::writeTextFileAtomically(config.auditOutputPath, serializePipelineAuditRecord(result))) {
            result.auditReason = "failed to write audit output";
        } else {
            result.auditOutputWritten = true;
            result.auditReason = "written";
        }
    }
    return result;
}

} // namespace strategic_nexus::strategic_pipeline
