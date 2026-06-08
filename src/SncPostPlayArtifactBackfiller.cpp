// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SncPostPlayArtifactBackfiller.h"

#include "SncGeneratedOverlayStager.h"
#include "common/FileUtil.h"
#include "common/JsonExtract.h"
#include "generated_overlay/MpOverlayPackage.h"

#include <cctype>
#include <filesystem>
#include <optional>
#include <regex>
#include <string>
#include <string_view>

namespace strategic_nexus {
namespace {

bool startsWith(const std::string& value, const std::string_view prefix)
{
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

std::optional<bool> extractJsonBool(const std::string& json, const char* key)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return std::nullopt;
    }
    return match[1].str() == "true";
}

std::string safeIdentifier(const std::string& value, const std::string& fallback)
{
    std::string output;
    output.reserve(value.size());
    bool lastWasSeparator = false;

    for (const unsigned char raw : value) {
        if (std::isalnum(raw) != 0) {
            output.push_back(static_cast<char>(std::tolower(raw)));
            lastWasSeparator = false;
            continue;
        }
        if (!lastWasSeparator && !output.empty()) {
            output.push_back('_');
            lastWasSeparator = true;
        }
    }

    while (!output.empty() && output.back() == '_') {
        output.pop_back();
    }
    if (output.empty()) {
        output = fallback;
    }
    if (std::isdigit(static_cast<unsigned char>(output.front())) != 0) {
        output = fallback + "_" + output;
    }
    if (output.size() > 80) {
        output.resize(80);
        while (!output.empty() && output.back() == '_') {
            output.pop_back();
        }
    }
    return output.empty() ? fallback : output;
}

void clearStaleZipIfPresent(const std::filesystem::path& zipPath)
{
    if (zipPath.empty()) {
        return;
    }

    std::error_code error;
    if (std::filesystem::exists(zipPath, error) && !error) {
        std::filesystem::remove(zipPath, error);
    }
}

bool isHealthyStagingStatus(
    const std::filesystem::path& statusPath,
    const std::filesystem::path& stagedOverlayDirectory)
{
    std::string json;
    if (!common::tryReadTextFile(statusPath, json)) {
        return false;
    }

    const auto ok = extractJsonBool(json, "ok");
    const auto readiness = common::extractJsonString(json, "readiness");
    const auto manifestVerified = extractJsonBool(json, "manifest_verified");
    if (!ok.value_or(false) || !manifestVerified.value_or(false) ||
        !readiness.has_value() || *readiness != "staged_verified") {
        return false;
    }

    std::error_code error;
    return std::filesystem::is_directory(stagedOverlayDirectory, error) && !error;
}

std::optional<std::string> readDslDraftReadiness(const std::filesystem::path& auditPath)
{
    std::string json;
    if (!common::tryReadTextFile(auditPath, json)) {
        return std::nullopt;
    }
    return common::extractJsonString(json, "readiness");
}

std::optional<std::string> extractCampaignIdFromCandidatePackage(
    const std::filesystem::path& candidatePackagePath)
{
    std::string json;
    if (!common::tryReadTextFile(candidatePackagePath, json)) {
        return std::nullopt;
    }

    const auto campaignKey = common::extractJsonString(json, "campaign_key");
    if (!campaignKey.has_value() || campaignKey->empty()) {
        return std::nullopt;
    }

    return safeIdentifier(*campaignKey, "campaign");
}

bool isCurrentMpPackageReady(
    const std::filesystem::path& packageDirectory,
    const std::string& expectedCampaignId,
    const std::string& expectedOverlayVersion,
    const std::string& expectedGameVersion,
    const std::string& expectedModVersion)
{
    generated_overlay::MpOverlayPackageVerifier verifier;
    const auto verification = verifier.verify(packageDirectory);
    if (!verification.ok) {
        return false;
    }

    return verification.campaignId == expectedCampaignId &&
           verification.overlayVersion == expectedOverlayVersion &&
           verification.gameVersion == expectedGameVersion &&
           verification.strategicNexusModVersion == expectedModVersion;
}

bool isCompatibleMpPackageReady(
    const std::filesystem::path& packageDirectory,
    const std::string& expectedOverlayVersion,
    const std::string& expectedGameVersion,
    const std::string& expectedModVersion)
{
    generated_overlay::MpOverlayPackageVerifier verifier;
    const auto verification = verifier.verify(packageDirectory);
    if (!verification.ok) {
        return false;
    }

    return verification.overlayVersion == expectedOverlayVersion &&
           verification.gameVersion == expectedGameVersion &&
           verification.strategicNexusModVersion == expectedModVersion;
}

} // namespace

SncPostPlayArtifactBackfillResult SncPostPlayArtifactBackfiller::backfill(
    const SncPostPlayArtifactBackfillConfig& config) const
{
    SncPostPlayArtifactBackfillResult result;

    if (config.dslDraftPath.empty() ||
        config.dslDraftAuditPath.empty() ||
        config.generatedOverlayStagingStatusPath.empty() ||
        config.generatedOverlayStagingDirectory.empty() ||
        config.mpOverlayPackageDirectory.empty()) {
        return result;
    }

    const auto dslDraftReadiness = readDslDraftReadiness(config.dslDraftAuditPath);
    if (!dslDraftReadiness.has_value() || !startsWith(*dslDraftReadiness, "ready")) {
        return result;
    }

    bool stagedOverlayReady = isHealthyStagingStatus(
        config.generatedOverlayStagingStatusPath,
        config.generatedOverlayStagingDirectory);

    if (!stagedOverlayReady) {
        result.attempted = true;

        const SncGeneratedOverlayStager stager;
        const auto stagingStatus = stager.stage(config.dslDraftPath, config.generatedOverlayStagingDirectory);
        result.stagedOverlayStatusWritten = common::writeTextFileAtomically(
            config.generatedOverlayStagingStatusPath,
            serializeSncGeneratedOverlayStagingStatus(stagingStatus));
        result.stagedOverlayReady = stagingStatus.ok;

        if (!result.stagedOverlayStatusWritten) {
            result.mpPackageRefreshState = "failed";
            result.mpPackageRefreshReason = "generated_overlay_staging_status_write_failed";
            clearStaleZipIfPresent(config.mpOverlayPackageZipPath);
            return result;
        }
        if (!stagingStatus.ok) {
            result.mpPackageRefreshState = "blocked";
            result.mpPackageRefreshReason =
                "generated_overlay_staging_backfill_failed: " + stagingStatus.reason;
            clearStaleZipIfPresent(config.mpOverlayPackageZipPath);
            return result;
        }

        stagedOverlayReady = true;
    }

    result.stagedOverlayReady = stagedOverlayReady;

    const auto campaignId = extractCampaignIdFromCandidatePackage(config.candidateDecisionPackagePath);
    if (!campaignId.has_value() || campaignId->empty()) {
        if (!isCompatibleMpPackageReady(
                config.mpOverlayPackageDirectory,
                config.mpOverlayVersion,
                config.mpGameVersion,
                config.mpModVersion)) {
            result.attempted = true;
            result.mpPackageRefreshState = "blocked";
            result.mpPackageRefreshReason = "missing_campaign_identity_for_mp_package_export";
            clearStaleZipIfPresent(config.mpOverlayPackageZipPath);
        } else {
            result.mpPackageRefreshState = "ready";
            result.mpPackageRefreshReason = "existing_mp_package_already_current";
        }
        return result;
    }

    if (isCurrentMpPackageReady(
            config.mpOverlayPackageDirectory,
            *campaignId,
            config.mpOverlayVersion,
            config.mpGameVersion,
            config.mpModVersion)) {
        result.mpPackageRefreshState = "ready";
        result.mpPackageRefreshReason = "existing_mp_package_already_current";
        return result;
    }

    result.attempted = true;
    clearStaleZipIfPresent(config.mpOverlayPackageZipPath);

    generated_overlay::MpOverlayPackageExporter exporter;
    const auto exportResult = exporter.exportPackage(
        config.generatedOverlayStagingDirectory,
        *campaignId,
        config.mpOverlayVersion,
        config.mpGameVersion,
        config.mpModVersion,
        config.mpOverlayPackageDirectory,
        false);
    if (!exportResult.ok) {
        result.mpPackageRefreshState = "failed";
        result.mpPackageRefreshReason = "mp_package_export_failed: " + exportResult.reason;
        return result;
    }

    result.mpOverlayPackageExported = true;
    result.mpPackageRefreshState = "ready";
    result.mpPackageRefreshReason = "mp_package_exported_from_existing_staged_overlay";
    return result;
}

} // namespace strategic_nexus
