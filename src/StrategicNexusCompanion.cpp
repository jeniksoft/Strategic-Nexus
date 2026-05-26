// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "StrategicNexusCompanion.h"

#include "generated_overlay/ManifestVerifier.h"

#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

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
    if (verification.ok) {
        status.state = "ready";
        status.reason = "generated overlay verified";
        return status;
    }

    if (verification.reason == "missing generated overlay manifest") {
        status.state = "starting";
        status.reason = "no generated overlay yet";
        return status;
    }

    status.state = "needs_attention";
    status.reason = verification.reason.empty() ? "generated overlay verification failed" : verification.reason;
    return status;
}

CompanionSubsystemStatus buildStatusCenterStatus(
    const CompanionSubsystemStatus& archive,
    const CompanionSubsystemStatus& generatedOverlay)
{
    CompanionSubsystemStatus status;
    status.state = "ready";
    status.reason = "no attention required";

    const bool archiveNeedsAttention =
        archive.state == "needs_setup" || archive.state == "needs_attention";
    const bool overlayNeedsAttention =
        generatedOverlay.state == "needs_setup" || generatedOverlay.state == "needs_attention";

    if (archiveNeedsAttention || overlayNeedsAttention) {
        status.state = "attention_required";
        status.reason = "archive or generated overlay needs attention";
        return status;
    }

    const bool archiveStarting = archive.state == "starting";
    const bool overlayStarting = generatedOverlay.state == "starting";

    if (archiveStarting || overlayStarting) {
        status.state = "starting";
        status.reason = "waiting for subsystems to become ready";
        return status;
    }

    return status;
}

void writeSubsystemJson(std::ostringstream& output, const CompanionSubsystemStatus& status, const std::string& indent)
{
    output << indent << "{\n";
    output << indent << "  \"state\": " << jsonString(status.state) << ",\n";
    output << indent << "  \"reason\": " << jsonString(status.reason) << ",\n";
    output << indent << "  \"path\": " << jsonString(pathString(status.path)) << "\n";
    output << indent << "}";
}

} // namespace

CompanionStatusSnapshot StrategicNexusCompanion::buildStatusSnapshot(const CompanionStatusConfig& config) const
{
    CompanionStatusSnapshot snapshot;
    snapshot.lifecycle.startWithWindowsEnabled = config.startWithWindowsEnabled;
    snapshot.archive = buildArchiveStatus(config.archiveRoot);
    snapshot.generatedOverlay = buildGeneratedOverlayStatus(config.generatedOverlayDirectory);
    snapshot.statusCenter = buildStatusCenterStatus(snapshot.archive, snapshot.generatedOverlay);
    return snapshot;
}

std::string serializeCompanionStatusSnapshot(const CompanionStatusSnapshot& snapshot)
{
    std::ostringstream output;
    output << "{\n";
    output << "  \"schema_version\": 1,\n";
    output << "  \"app_name\": " << jsonString(snapshot.appName) << ",\n";
    output << "  \"abbreviation\": " << jsonString(snapshot.abbreviation) << ",\n";
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
    output << "  \"generated_overlay_status\": ";
    writeSubsystemJson(output, snapshot.generatedOverlay, "  ");
    output << ",\n";
    output << "  \"status_center\": ";
    writeSubsystemJson(output, snapshot.statusCenter, "  ");
    output << "\n";
    output << "}\n";
    return output.str();
}

} // namespace strategic_nexus
