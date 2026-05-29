// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "StrategicNexusCompanion.h"

#include "common/FileUtil.h"
#include "generated_overlay/ManifestVerifier.h"
#include "generated_overlay/MpOverlayPackage.h"
#include "StellarisProcessDetector.h"
#include "StellarisSavePathResolver.h"

#include <filesystem>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>

namespace strategic_nexus {
namespace {

std::string escapeJson(const std::string& value)
{
    std::ostringstream output;
    for (const unsigned char raw : value) {
        const char ch = static_cast<char>(raw);
        switch (ch) {
        case '\\':
            output << "\\\\";
            break;
        case '"':
            output << "\\\"";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        default:
            if (raw < 0x20) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<int>(raw) << std::dec << std::setw(0);
            } else {
                output << ch;
            }
            break;
        }
    }
    return output.str();
}

std::string jsonString(const std::string& value)
{
    return "\"" + escapeJson(value) + "\"";
}

std::string pathString(const std::filesystem::path& path)
{
    return path.generic_string();
}

std::string formatLocalTimestamp(std::chrono::system_clock::time_point now)
{
    const std::time_t timeValue = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    if (localtime_s(&localTime, &timeValue) != 0) {
        return std::string();
    }

    std::ostringstream output;
    output << localTime.tm_mday << "." << (localTime.tm_mon + 1) << "." << (localTime.tm_year + 1900) << "  "
           << localTime.tm_hour << ":" << std::setw(2) << std::setfill('0') << localTime.tm_min << ":"
           << std::setw(2) << std::setfill('0') << localTime.tm_sec;
    return output.str();
}

CompanionSubsystemStatus buildSaveDiscoveryStatus()
{
    CompanionSubsystemStatus status;
    status.state = "starting";
    status.reason = "discovering Stellaris save roots";

    const StellarisSavePathResolver resolver;
    const auto discovery = resolver.discoverFromEnvironment();

    if (discovery.candidates.empty()) {
        status.state = "needs_setup";
        status.reason = "save discovery unavailable";
        return status;
    }

    status.path = discovery.candidates.front().path;

    for (const auto& candidate : discovery.candidates) {
        if (candidate.exists) {
            status.state = "ready";
            status.reason = "Stellaris save root discovered";
            status.path = candidate.path;
            return status;
        }
    }

    status.state = "needs_attention";
    status.reason = "no Stellaris save roots found";
    return status;
}

CompanionSubsystemStatus buildArchiveStatus(const std::filesystem::path& archiveRoot)
{
    CompanionSubsystemStatus status;
    status.path = archiveRoot;

    if (archiveRoot.empty()) {
        status.state = "needs_setup";
        status.reason = "archive root not configured";
        return status;
    }

    std::error_code error;
    const bool exists = std::filesystem::exists(archiveRoot, error);
    if (error) {
        status.state = "needs_attention";
        status.reason = "archive root inaccessible";
        return status;
    }

    if (!exists) {
        status.state = "needs_attention";
        status.reason = "archive root missing";
        return status;
    }

    const bool isDirectory = std::filesystem::is_directory(archiveRoot, error);
    if (error) {
        status.state = "needs_attention";
        status.reason = "archive root inaccessible";
        return status;
    }

    if (!isDirectory) {
        status.state = "needs_attention";
        status.reason = "archive root is not a directory";
        return status;
    }

    if (std::filesystem::exists(archiveRoot / "manifest.json", error) && !error) {
        status.state = "ready";
        status.reason = "archive session manifest available";
        return status;
    }

    bool hasSessions = false;
    for (const auto& entry : std::filesystem::directory_iterator(archiveRoot, error)) {
        if (error) {
            break;
        }
        if (!entry.is_directory(error)) {
            continue;
        }
        if (std::filesystem::exists(entry.path() / "manifest.json", error) && !error) {
            hasSessions = true;
            break;
        }
    }

    if (!hasSessions) {
        status.state = "starting";
        status.reason = "no archive sessions yet";
        return status;
    }

    status.state = "ready";
    status.reason = "archive sessions available";
    return status;
}

CompanionSubsystemStatus buildGeneratedOverlayStatus(const std::filesystem::path& overlayDirectory)
{
    CompanionSubsystemStatus status;
    status.path = overlayDirectory;

    if (overlayDirectory.empty()) {
        status.state = "needs_setup";
        status.reason = "generated overlay directory not configured";
        return status;
    }

    std::error_code error;
    const bool exists = std::filesystem::exists(overlayDirectory, error);
    if (error) {
        status.state = "needs_attention";
        status.reason = "generated overlay directory inaccessible";
        return status;
    }

    if (!exists) {
        status.state = "needs_attention";
        status.reason = "generated overlay directory missing";
        return status;
    }

    const bool isDirectory = std::filesystem::is_directory(overlayDirectory, error);
    if (error) {
        status.state = "needs_attention";
        status.reason = "generated overlay directory inaccessible";
        return status;
    }

    if (!isDirectory) {
        status.state = "needs_attention";
        status.reason = "generated overlay path is not a directory";
        return status;
    }

    const generated_overlay::ManifestVerifier verifier;
    const auto verification = verifier.verify(overlayDirectory);
    status.manifestHash = verification.manifestHash;
    if (verification.ok) {
        status.state = "ready";
        status.reason = "generated overlay verified";
        return status;
    }

    if (verification.reason == "missing generated overlay manifest") {
        std::error_code iterError;
        const bool hasAnyEntry =
            std::filesystem::directory_iterator(overlayDirectory, iterError) != std::filesystem::directory_iterator();
        if (iterError) {
            status.state = "needs_attention";
            status.reason = "generated overlay directory inaccessible";
            return status;
        }

        if (!hasAnyEntry) {
            status.state = "starting";
            status.reason = "no generated overlay yet";
            return status;
        }

        status.state = "needs_attention";
        status.reason = "missing generated overlay manifest";
        return status;
    }

    status.state = "needs_attention";
    status.reason = verification.reason.empty() ? "generated overlay verification failed" : verification.reason;
    return status;
}

CompanionMpOverlayPackageStatus buildMpOverlayPackageStatus(const std::filesystem::path& packageDirectory)
{
    CompanionMpOverlayPackageStatus status;
    status.path = packageDirectory;

    if (packageDirectory.empty()) {
        status.state = "disabled";
        status.reason = "mp overlay package directory not configured";
        return status;
    }

    std::error_code error;
    const bool exists = std::filesystem::exists(packageDirectory, error);
    if (error) {
        status.state = "needs_attention";
        status.reason = "mp overlay package directory inaccessible";
        return status;
    }

    if (!exists) {
        status.state = "needs_attention";
        status.reason = "mp overlay package directory missing";
        return status;
    }

    const bool isDirectory = std::filesystem::is_directory(packageDirectory, error);
    if (error) {
        status.state = "needs_attention";
        status.reason = "mp overlay package directory inaccessible";
        return status;
    }

    if (!isDirectory) {
        status.state = "needs_attention";
        status.reason = "mp overlay package path is not a directory";
        return status;
    }

    const generated_overlay::MpOverlayPackageVerifier verifier;
    const auto verification = verifier.verify(packageDirectory);
    status.campaignId = verification.campaignId;
    status.overlayVersion = verification.overlayVersion;
    status.gameVersion = verification.gameVersion;
    status.strategicNexusModVersion = verification.strategicNexusModVersion;
    status.handoffStatus = verification.handoffStatus;
    status.readiness = verification.readiness;
    status.packageManifestHash = verification.packageManifestHash;
    if (verification.ok) {
        status.state = "ready";
        status.reason = "mp overlay package verified";
        status.statusText = verification.statusText;
        return status;
    }

    status.state = "needs_attention";
    status.reason = verification.reason.empty() ? "mp overlay package verification failed" : verification.reason;
    status.statusText = verification.statusText;
    return status;
}

CompanionSubsystemStatus buildGeneratedOverlayPublishGateStatus(
    const CompanionSubsystemStatus& generatedOverlay,
    const CompanionStatusConfig& config)
{
    CompanionSubsystemStatus status;
    status.path = generatedOverlay.path;

    if (generatedOverlay.state == "needs_setup" || generatedOverlay.state == "needs_attention") {
        status.state = "needs_attention";
        status.reason = "generated overlay is not publishable";
        return status;
    }

    if (generatedOverlay.state == "starting") {
        status.state = "starting";
        status.reason = "waiting for generated overlay before publish";
        return status;
    }

    StellarisProcessStatus processStatus;
    if (config.useDetectedStellarisState) {
        const StellarisProcessDetector detector;
        processStatus = detector.detectFromSystem();
    } else {
        processStatus.detectionAvailable = true;
        processStatus.running = config.stellarisRunningOverride;
        processStatus.reason =
            config.stellarisRunningOverride ? "explicit Stellaris running override" : "explicit Stellaris not running override";
    }

    if (!processStatus.detectionAvailable) {
        status.state = "needs_attention";
        status.reason = "Stellaris process detection unavailable; generated overlay publish blocked";
        return status;
    }

    if (processStatus.running) {
        status.state = "blocked";
        status.reason = "Stellaris is running; generated overlay publish deferred";
        return status;
    }

    status.state = "ready";
    status.reason = "Stellaris is not running; generated overlay publish allowed";
    return status;
}

CompanionSubsystemStatus buildStatusCenterStatus(
    const CompanionSubsystemStatus& saveDiscovery,
    const CompanionSubsystemStatus& archive,
    const CompanionSubsystemStatus& generatedOverlay,
    const CompanionSubsystemStatus& generatedOverlayPublishGate,
    const CompanionMpOverlayPackageStatus& mpOverlayPackage)
{
    CompanionSubsystemStatus status;
    status.state = "ready";
    status.reason = "no attention required";

    const bool archiveNeedsAttention =
        archive.state == "needs_setup" || archive.state == "needs_attention";
    const bool overlayNeedsAttention =
        generatedOverlay.state == "needs_setup" || generatedOverlay.state == "needs_attention";
    const bool publishGateNeedsAttention =
        generatedOverlayPublishGate.state == "needs_setup" || generatedOverlayPublishGate.state == "needs_attention";
    const bool mpNeedsAttention = mpOverlayPackage.state == "needs_attention";

    if (archiveNeedsAttention || overlayNeedsAttention || publishGateNeedsAttention || mpNeedsAttention) {
        status.state = "attention_required";
        if (archiveNeedsAttention && overlayNeedsAttention && publishGateNeedsAttention && mpNeedsAttention) {
            status.reason = "archive, generated overlay, publish gate, and mp overlay package need attention";
            return status;
        }
        if (archiveNeedsAttention && overlayNeedsAttention) {
            status.reason = "archive and generated overlay need attention";
            return status;
        }
        if (archiveNeedsAttention && mpNeedsAttention) {
            status.reason = "archive and mp overlay package need attention";
            return status;
        }
        if (overlayNeedsAttention && mpNeedsAttention) {
            status.reason = "generated overlay and mp overlay package need attention";
            return status;
        }
        if (publishGateNeedsAttention && mpNeedsAttention) {
            status.reason = "generated overlay publish gate and mp overlay package need attention";
            return status;
        }
        if (archiveNeedsAttention) {
            status.reason = "archive needs attention";
            status.path = archive.path;
            if (archive.state == "needs_setup" && !saveDiscovery.path.empty()) {
                status.path = saveDiscovery.path;
            }
            return status;
        }
        if (overlayNeedsAttention) {
            status.reason = "generated overlay needs attention";
            status.path = generatedOverlay.path;
            return status;
        }
        if (publishGateNeedsAttention) {
            status.reason = "generated overlay publish gate needs attention";
            status.path = generatedOverlayPublishGate.path;
            return status;
        }
        status.reason = "mp overlay package needs attention";
        status.path = mpOverlayPackage.path;
        return status;
    }

    const bool archiveStarting = archive.state == "starting";
    const bool overlayStarting = generatedOverlay.state == "starting";
    const bool publishGateStarting = generatedOverlayPublishGate.state == "starting";

    if (generatedOverlayPublishGate.state == "blocked") {
        status.state = "waiting";
        status.reason = generatedOverlayPublishGate.reason;
        status.path = generatedOverlayPublishGate.path;
        return status;
    }

    if (archiveStarting || overlayStarting || publishGateStarting) {
        status.state = "starting";
        if (archiveStarting && overlayStarting && publishGateStarting) {
            status.reason = "waiting for archive, generated overlay, and publish gate to become ready";
            return status;
        }
        if (archiveStarting && overlayStarting) {
            status.reason = "waiting for archive and generated overlay to become ready";
            return status;
        }
        if (archiveStarting && publishGateStarting) {
            status.reason = "waiting for archive and generated overlay publish gate to become ready";
            return status;
        }
        if (overlayStarting && publishGateStarting) {
            status.reason = "waiting for generated overlay and publish gate to become ready";
            return status;
        }
        if (archiveStarting) {
            status.reason = "waiting for archive to become ready";
            status.path = archive.path;
            return status;
        }
        if (publishGateStarting) {
            status.reason = "waiting for generated overlay publish gate to become ready";
            status.path = generatedOverlayPublishGate.path;
            return status;
        }
        status.reason = "waiting for generated overlay to become ready";
        status.path = generatedOverlay.path;
        return status;
    }

    return status;
}

std::string buildStatusCenterSummaryText(
    const CompanionSubsystemStatus& saveDiscovery,
    const CompanionSubsystemStatus& archive,
    const CompanionSubsystemStatus& generatedOverlay,
    const CompanionSubsystemStatus& generatedOverlayPublishGate,
    const CompanionMpOverlayPackageStatus& mpOverlayPackage,
    const CompanionSubsystemStatus& statusCenter)
{
    std::ostringstream text;
    text << "Strategic Nexus Status Center\n";
    text << "stav: " << statusCenter.state << " - " << statusCenter.reason << "\n";
    text << "nalezeni_uloziste: " << saveDiscovery.state << " - " << saveDiscovery.reason << "\n";
    text << "archiv: " << archive.state << " - " << archive.reason << "\n";
    text << "generovany_overlay: " << generatedOverlay.state << " - " << generatedOverlay.reason << "\n";
    text << "publish_gate: " << generatedOverlayPublishGate.state << " - " << generatedOverlayPublishGate.reason << "\n";
    text << "mp_overlay_balicek: " << mpOverlayPackage.state << " - " << mpOverlayPackage.reason << "\n";

    if (!generatedOverlay.manifestHash.empty()) {
        text << "generated_overlay_manifest_hash: " << generatedOverlay.manifestHash << "\n";
    }
    if (!mpOverlayPackage.campaignId.empty()) {
        text << "campaign_id: " << mpOverlayPackage.campaignId << "\n";
    }
    if (!mpOverlayPackage.overlayVersion.empty()) {
        text << "overlay_version: " << mpOverlayPackage.overlayVersion << "\n";
    }
    if (!mpOverlayPackage.gameVersion.empty()) {
        text << "game_version: " << mpOverlayPackage.gameVersion << "\n";
    }
    if (!mpOverlayPackage.strategicNexusModVersion.empty()) {
        text << "strategic_nexus_mod_version: " << mpOverlayPackage.strategicNexusModVersion << "\n";
    }
    if (!mpOverlayPackage.handoffStatus.empty()) {
        text << "handoff_status: " << mpOverlayPackage.handoffStatus << "\n";
    }
    if (!mpOverlayPackage.readiness.empty()) {
        text << "mp_readiness: " << mpOverlayPackage.readiness << "\n";
    }
    if (!mpOverlayPackage.packageManifestHash.empty()) {
        text << "package_manifest_hash: " << mpOverlayPackage.packageManifestHash << "\n";
    }
    return text.str();
}

void writeSubsystemJson(
    std::ostringstream& output,
    const CompanionSubsystemStatus& status,
    const std::string& indent,
    const bool includeManifestHash = false)
{
    output << indent << "{\n";
    output << indent << "  \"state\": " << jsonString(status.state) << ",\n";
    output << indent << "  \"reason\": " << jsonString(status.reason) << ",\n";
    output << indent << "  \"path\": " << jsonString(pathString(status.path));
    if (includeManifestHash) {
        output << ",\n";
        output << indent << "  \"manifest_hash\": " << jsonString(status.manifestHash) << "\n";
    } else {
        output << "\n";
    }
    output << indent << "}";
}

void writeMpOverlayPackageJson(std::ostringstream& output, const CompanionMpOverlayPackageStatus& status, const std::string& indent)
{
    output << indent << "{\n";
    output << indent << "  \"state\": " << jsonString(status.state) << ",\n";
    output << indent << "  \"reason\": " << jsonString(status.reason) << ",\n";
    output << indent << "  \"path\": " << jsonString(pathString(status.path)) << ",\n";
    output << indent << "  \"campaign_id\": " << jsonString(status.campaignId) << ",\n";
    output << indent << "  \"overlay_version\": " << jsonString(status.overlayVersion) << ",\n";
    output << indent << "  \"game_version\": " << jsonString(status.gameVersion) << ",\n";
    output << indent << "  \"strategic_nexus_mod_version\": " << jsonString(status.strategicNexusModVersion) << ",\n";
    output << indent << "  \"handoff_status\": " << jsonString(status.handoffStatus) << ",\n";
    output << indent << "  \"readiness\": " << jsonString(status.readiness) << ",\n";
    output << indent << "  \"package_manifest_hash\": " << jsonString(status.packageManifestHash) << ",\n";
    output << indent << "  \"status_text\": " << jsonString(status.statusText) << "\n";
    output << indent << "}";
}

} // namespace

CompanionStatusSnapshot StrategicNexusCompanion::buildStatusSnapshot(const CompanionStatusConfig& config) const
{
    CompanionStatusSnapshot snapshot;
    snapshot.lifecycle.startWithWindowsEnabled = config.startWithWindowsEnabled;
    snapshot.generatedAtLocal = formatLocalTimestamp(std::chrono::system_clock::now());
    snapshot.saveDiscovery = buildSaveDiscoveryStatus();
    snapshot.archive = buildArchiveStatus(config.archiveRoot);
    snapshot.generatedOverlay = buildGeneratedOverlayStatus(config.generatedOverlayDirectory);
    snapshot.generatedOverlayPublishGate = buildGeneratedOverlayPublishGateStatus(snapshot.generatedOverlay, config);
    snapshot.mpOverlayPackage = buildMpOverlayPackageStatus(config.mpOverlayPackageDirectory);
    snapshot.statusCenter =
        buildStatusCenterStatus(
            snapshot.saveDiscovery,
            snapshot.archive,
            snapshot.generatedOverlay,
            snapshot.generatedOverlayPublishGate,
            snapshot.mpOverlayPackage);
    snapshot.statusCenterSummaryText = buildStatusCenterSummaryText(
        snapshot.saveDiscovery,
        snapshot.archive,
        snapshot.generatedOverlay,
        snapshot.generatedOverlayPublishGate,
        snapshot.mpOverlayPackage,
        snapshot.statusCenter);
    return snapshot;
}

std::string serializeCompanionStatusSnapshot(const CompanionStatusSnapshot& snapshot)
{
    std::ostringstream output;
    output << "{\n";
    output << "  \"schema_version\": 1,\n";
    output << "  \"app_name\": " << jsonString(snapshot.appName) << ",\n";
    output << "  \"abbreviation\": " << jsonString(snapshot.abbreviation) << ",\n";
    output << "  \"generated_at_local\": " << jsonString(snapshot.generatedAtLocal) << ",\n";
    output << "  \"lifecycle\": {\n";
    output << "    \"start_with_windows_enabled\": "
           << (snapshot.lifecycle.startWithWindowsEnabled ? "true" : "false") << ",\n";
    output << "    \"window_close_behavior\": "
           << jsonString(snapshot.lifecycle.windowCloseBehavior) << ",\n";
    output << "    \"explicit_exit_behavior\": "
           << jsonString(snapshot.lifecycle.explicitExitBehavior) << ",\n";
    output << "    \"crash_restart_policy\": "
           << jsonString(snapshot.lifecycle.crashRestartPolicy) << "\n";
    output << "  },\n";
    output << "  \"archive_status\": ";
    writeSubsystemJson(output, snapshot.archive, "  ");
    output << ",\n";
    output << "  \"save_discovery_status\": ";
    writeSubsystemJson(output, snapshot.saveDiscovery, "  ");
    output << ",\n";
    output << "  \"generated_overlay_status\": ";
    writeSubsystemJson(output, snapshot.generatedOverlay, "  ", true);
    output << ",\n";
    output << "  \"generated_overlay_publish_gate_status\": ";
    writeSubsystemJson(output, snapshot.generatedOverlayPublishGate, "  ");
    output << ",\n";
    output << "  \"mp_overlay_package_status\": ";
    writeMpOverlayPackageJson(output, snapshot.mpOverlayPackage, "  ");
    output << ",\n";
    output << "  \"status_center\": ";
    writeSubsystemJson(output, snapshot.statusCenter, "  ");
    output << ",\n";
    output << "  \"status_center_summary_text\": " << jsonString(snapshot.statusCenterSummaryText) << "\n";
    output << "}\n";
    return output.str();
}

CompanionStatusLoopResult runCompanionStatusLoop(const StrategicNexusCompanion& companion, const CompanionStatusLoopConfig& config)
{
    CompanionStatusLoopResult result;

    if (config.statusOutputPath.empty()) {
        result.reason = "status output path not configured";
        return result;
    }

    std::error_code outputTypeError;
    const bool outputIsDirectory = std::filesystem::is_directory(config.statusOutputPath, outputTypeError);
    if (outputTypeError) {
        const bool missingPath = outputTypeError == std::errc::no_such_file_or_directory;
        if (!missingPath) {
            result.reason = "status output path inaccessible: " + config.statusOutputPath.generic_string();
            return result;
        }
        outputTypeError.clear();
    }
    if (outputIsDirectory) {
        result.reason = "status output path is a directory: " + config.statusOutputPath.generic_string();
        return result;
    }

    const int iterations = config.maxIterations < 0 ? 0 : config.maxIterations;
    if (iterations == 0) {
        result.ok = true;
        result.reason = "no iterations requested";
        return result;
    }

    for (int i = 0; i < iterations; i++) {
        const auto snapshot = companion.buildStatusSnapshot(config.statusConfig);
        const auto json = serializeCompanionStatusSnapshot(snapshot);
        const bool written = common::writeTextFileAtomically(config.statusOutputPath, json);
        if (!written) {
            result.reason = "failed to write status snapshot: " + config.statusOutputPath.generic_string();
            return result;
        }

        result.iterationsRun++;
        if (i + 1 < iterations && config.pollInterval.count() > 0) {
            std::this_thread::sleep_for(config.pollInterval);
        }
    }

    result.ok = true;
    result.reason = "ok";
    return result;
}

} // namespace strategic_nexus
