// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SncGeneratedOverlayStager.h"

#include "common/FileUtil.h"
#include "generated_overlay/DslParser.h"
#include "generated_overlay/DslValidator.h"
#include "generated_overlay/ManifestVerifier.h"
#include "generated_overlay/OverlayCompiler.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>

namespace strategic_nexus {
namespace {

std::string jsonEscape(const std::string_view value)
{
    std::ostringstream output;
    for (const char ch : value) {
        switch (ch) {
        case '\\': output << "\\\\"; break;
        case '"': output << "\\\""; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20) {
                output << ' ';
            } else {
                output << ch;
            }
            break;
        }
    }
    return output.str();
}

void writeStringArray(
    std::ostringstream& json,
    const std::vector<std::string>& values,
    const int indent)
{
    const std::string padding(static_cast<std::size_t>(indent), ' ');
    json << "[";
    if (!values.empty()) {
        json << "\n";
    }
    for (std::size_t index = 0; index < values.size(); ++index) {
        json << padding << "  \"" << jsonEscape(values[index]) << "\"";
        json << (index + 1 < values.size() ? "," : "") << "\n";
    }
    if (!values.empty()) {
        json << padding;
    }
    json << "]";
}

bool isDangerousReplacementDirectory(const std::filesystem::path& path)
{
    if (path.empty()) {
        return true;
    }

    const auto normalized = path.lexically_normal();
    const auto fileName = normalized.filename().generic_string();
    if (fileName.empty() || fileName == "." || fileName == "..") {
        return true;
    }

    if (!normalized.has_parent_path()) {
        return true;
    }

    const auto root = normalized.root_path();
    if (!root.empty() && normalized == root) {
        return true;
    }

    std::error_code absoluteError;
    const auto absolutePath = std::filesystem::absolute(normalized, absoluteError).lexically_normal();
    if (absoluteError) {
        return true;
    }

    return absolutePath == absolutePath.root_path();
}

bool prepareCleanDirectory(const std::filesystem::path& directory, std::string& reason)
{
    if (isDangerousReplacementDirectory(directory)) {
        reason = "refusing dangerous staging directory";
        return false;
    }

    std::error_code error;
    if (std::filesystem::exists(directory, error)) {
        if (error) {
            reason = "failed to inspect staging directory";
            return false;
        }
        if (!std::filesystem::is_directory(directory, error) || error) {
            reason = "staging path is not a directory";
            return false;
        }
        std::filesystem::remove_all(directory, error);
        if (error) {
            reason = "failed to clean staging directory";
            return false;
        }
    }

    std::filesystem::create_directories(directory, error);
    if (error) {
        reason = "failed to create staging directory";
        return false;
    }

    return true;
}

void addWarning(SncGeneratedOverlayStagingStatus& status, const std::string& warningCode)
{
    status.warningCodes.push_back(warningCode);
}

} // namespace

SncGeneratedOverlayStagingStatus SncGeneratedOverlayStager::stage(
    const std::filesystem::path& sourceDslPath,
    const std::filesystem::path& stagedOverlayDirectory) const
{
    SncGeneratedOverlayStagingStatus status;
    status.reason = "not started";
    status.sourceDslPath = sourceDslPath;
    status.stagedOverlayDirectory = stagedOverlayDirectory;

    if (sourceDslPath.empty() || stagedOverlayDirectory.empty()) {
        status.reason = "missing DSL input path or staging directory";
        addWarning(status, "snc_generated_overlay_staging_missing_path");
        return status;
    }

    std::string dslText;
    if (!common::tryReadTextFile(sourceDslPath, dslText)) {
        status.reason = "failed to read DSL input";
        addWarning(status, "snc_generated_overlay_staging_read_failed");
        return status;
    }

    const generated_overlay::DslParser parser;
    const auto parseResult = parser.parse(dslText);
    if (!parseResult.ok) {
        status.reason = "parse failed";
        status.validationErrors.push_back(parseResult.error);
        addWarning(status, "snc_generated_overlay_staging_parse_failed");
        return status;
    }

    status.dslRuleCount = parseResult.program.rules.size();
    if (status.dslRuleCount == 0) {
        status.reason = "DSL input has no rules";
        addWarning(status, "snc_generated_overlay_staging_no_rules");
        return status;
    }

    const generated_overlay::DslValidator validator;
    const auto validation = validator.validate(parseResult.program);
    status.validatorPassed = validation.ok;
    status.validationErrors = validation.errors;
    if (!validation.ok) {
        status.reason = "validation failed";
        addWarning(status, "snc_generated_overlay_staging_validation_failed");
        return status;
    }

    const auto runtimeConditionErrors = generated_overlay::findUnsupportedRuntimeConditionErrors(parseResult.program);
    if (!runtimeConditionErrors.empty()) {
        status.reason = "generated overlay runtime conditions are not safely enforceable yet";
        status.validationErrors = runtimeConditionErrors;
        addWarning(status, "snc_generated_overlay_staging_runtime_conditions_unsupported");
        return status;
    }

    std::string prepareReason;
    if (!prepareCleanDirectory(stagedOverlayDirectory, prepareReason)) {
        status.reason = prepareReason;
        addWarning(status, "snc_generated_overlay_staging_directory_failed");
        return status;
    }

    const generated_overlay::OverlayCompiler compiler;
    const auto files = compiler.compile(parseResult.program);
    status.eventsWritten = common::writeTextFileAtomically(
        stagedOverlayDirectory / "events" / "strategic_nexus_generated_events.txt",
        files.eventsText);
    status.effectsWritten = common::writeTextFileAtomically(
        stagedOverlayDirectory / "common" / "scripted_effects" / "strategic_nexus_generated_effects.txt",
        files.scriptedEffectsText);
    status.triggersWritten = common::writeTextFileAtomically(
        stagedOverlayDirectory / "common" / "scripted_triggers" / "strategic_nexus_generated_triggers.txt",
        files.scriptedTriggersText);
    status.manifestWritten = common::writeTextFileAtomically(
        stagedOverlayDirectory / "strategic_nexus_generated_manifest.json",
        files.manifestText);

    if (!status.eventsWritten || !status.effectsWritten || !status.triggersWritten || !status.manifestWritten) {
        status.reason = "failed to write staged generated overlay files";
        addWarning(status, "snc_generated_overlay_staging_write_failed");
        return status;
    }

    const generated_overlay::ManifestVerifier verifier;
    const auto verification = verifier.verify(stagedOverlayDirectory);
    status.manifestVerified = verification.ok;
    status.manifestHash = verification.manifestHash;
    status.manifestFileCount = verification.files.size();
    status.unexpectedFileCount = verification.unexpectedFiles.size();
    if (!verification.ok) {
        status.reason = "manifest verification failed: " + verification.reason;
        addWarning(status, "snc_generated_overlay_staging_manifest_failed");
        return status;
    }

    status.ok = true;
    status.reason = "validated generated overlay staged";
    status.readiness = "staged_verified";
    status.publishAllowed = false;
    status.publishesOverlay = false;
    return status;
}

std::string serializeSncGeneratedOverlayStagingStatus(const SncGeneratedOverlayStagingStatus& status)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"ok\": " << (status.ok ? "true" : "false") << ",\n";
    json << "  \"reason\": \"" << jsonEscape(status.reason) << "\",\n";
    json << "  \"readiness\": \"" << jsonEscape(status.readiness) << "\",\n";
    json << "  \"dry_run_only\": " << (status.dryRunOnly ? "true" : "false") << ",\n";
    json << "  \"publish_allowed\": " << (status.publishAllowed ? "true" : "false") << ",\n";
    json << "  \"publishes_overlay\": " << (status.publishesOverlay ? "true" : "false") << ",\n";
    json << "  \"source_dsl_path\": \"" << jsonEscape(status.sourceDslPath.generic_string()) << "\",\n";
    json << "  \"staged_overlay_directory\": \"" << jsonEscape(status.stagedOverlayDirectory.generic_string()) << "\",\n";
    json << "  \"dsl_rule_count\": " << status.dslRuleCount << ",\n";
    json << "  \"validator_passed\": " << (status.validatorPassed ? "true" : "false") << ",\n";
    json << "  \"manifest_verified\": " << (status.manifestVerified ? "true" : "false") << ",\n";
    json << "  \"manifest_hash\": \"" << jsonEscape(status.manifestHash) << "\",\n";
    json << "  \"manifest_file_count\": " << status.manifestFileCount << ",\n";
    json << "  \"unexpected_file_count\": " << status.unexpectedFileCount << ",\n";
    json << "  \"events_written\": " << (status.eventsWritten ? "true" : "false") << ",\n";
    json << "  \"effects_written\": " << (status.effectsWritten ? "true" : "false") << ",\n";
    json << "  \"triggers_written\": " << (status.triggersWritten ? "true" : "false") << ",\n";
    json << "  \"manifest_written\": " << (status.manifestWritten ? "true" : "false") << ",\n";
    json << "  \"warning_codes\": ";
    writeStringArray(json, status.warningCodes, 2);
    json << ",\n";
    json << "  \"validation_errors\": ";
    writeStringArray(json, status.validationErrors, 2);
    json << "\n";
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus
