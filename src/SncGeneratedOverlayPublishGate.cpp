// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SncGeneratedOverlayPublishGate.h"

#include "common/FileUtil.h"
#include "common/JsonExtract.h"
#include "generated_overlay/GeneratedOverlayPublisher.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace strategic_nexus {
namespace {

constexpr const char* ownerApprovalToken = "owner-approved";

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

std::optional<bool> extractJsonBool(const std::string& json, const char* key)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return std::nullopt;
    }
    return match[1].str() == "true";
}

bool samePath(const std::filesystem::path& left, const std::filesystem::path& right)
{
    std::error_code error;
    const auto canonicalLeft = std::filesystem::weakly_canonical(left, error);
    if (error) {
        return false;
    }
    const auto canonicalRight = std::filesystem::weakly_canonical(right, error);
    if (error) {
        return false;
    }
    return canonicalLeft == canonicalRight;
}

bool isDangerousDirectoryTarget(const std::filesystem::path& path)
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

void addWarning(SncGeneratedOverlayPublishGateResult& result, const std::string& warningCode)
{
    result.warningCodes.push_back(warningCode);
}

std::string timestampToken()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t timeValue = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    if (localtime_s(&localTime, &timeValue) != 0) {
        return "unknown-time";
    }

    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() % 1000;

    std::ostringstream output;
    output << std::setw(4) << std::setfill('0') << (localTime.tm_year + 1900)
           << std::setw(2) << std::setfill('0') << (localTime.tm_mon + 1)
           << std::setw(2) << std::setfill('0') << localTime.tm_mday
           << "T"
           << std::setw(2) << std::setfill('0') << localTime.tm_hour
           << std::setw(2) << std::setfill('0') << localTime.tm_min
           << std::setw(2) << std::setfill('0') << localTime.tm_sec
           << std::setw(3) << std::setfill('0') << milliseconds;
    return output.str();
}

std::filesystem::path defaultBackupRootFor(const std::filesystem::path& activeDirectory)
{
    const auto parent = activeDirectory.parent_path();
    if (parent.empty()) {
        return std::filesystem::path("snc_generated_overlay_backups");
    }
    return parent / "snc_generated_overlay_backups";
}

std::filesystem::path uniqueBackupDirectory(
    const std::filesystem::path& backupRoot,
    const std::filesystem::path& activeDirectory)
{
    const auto activeName = activeDirectory.filename().empty()
        ? std::filesystem::path("generated_overlay")
        : activeDirectory.filename();
    const auto base = backupRoot / (activeName.string() + "_" + timestampToken());

    std::filesystem::path candidate = base;
    for (int suffix = 2; std::filesystem::exists(candidate); ++suffix) {
        candidate = std::filesystem::path(base.string() + "_" + std::to_string(suffix));
    }
    return candidate;
}

bool backupActiveDirectory(
    const std::filesystem::path& activeDirectory,
    const std::filesystem::path& backupRoot,
    std::filesystem::path& backupDirectory,
    std::string& reason)
{
    std::error_code error;
    if (!std::filesystem::exists(activeDirectory, error)) {
        return true;
    }
    if (error) {
        reason = "failed to inspect active generated overlay directory";
        return false;
    }
    if (!std::filesystem::is_directory(activeDirectory, error) || error) {
        reason = "active generated overlay path is not a directory";
        return false;
    }

    if (isDangerousDirectoryTarget(backupRoot) ||
        samePath(activeDirectory, backupRoot)) {
        reason = "unsafe generated overlay backup directory";
        return false;
    }

    std::filesystem::create_directories(backupRoot, error);
    if (error) {
        reason = "failed to create generated overlay backup root";
        return false;
    }

    backupDirectory = uniqueBackupDirectory(backupRoot, activeDirectory);
    std::filesystem::copy(
        activeDirectory,
        backupDirectory,
        std::filesystem::copy_options::recursive,
        error);
    if (error) {
        reason = "failed to backup active generated overlay directory";
        return false;
    }

    return true;
}

bool validateStagingStatus(
    const std::string& statusText,
    SncGeneratedOverlayPublishGateResult& result)
{
    const auto ok = extractJsonBool(statusText, "ok");
    if (!ok.has_value() || !*ok) {
        result.reason = "staging status is not successful";
        addWarning(result, "snc_generated_overlay_publish_staging_not_ok");
        return false;
    }

    const auto readiness = common::extractJsonString(statusText, "readiness");
    if (!readiness.has_value() || *readiness != "staged_verified") {
        result.reason = "staging status is not staged_verified";
        addWarning(result, "snc_generated_overlay_publish_staging_not_verified");
        return false;
    }
    result.readiness = *readiness;

    const auto manifestVerified = extractJsonBool(statusText, "manifest_verified");
    if (!manifestVerified.has_value() || !*manifestVerified) {
        result.reason = "staging manifest is not verified";
        addWarning(result, "snc_generated_overlay_publish_manifest_not_verified");
        return false;
    }

    const auto publishAllowed = extractJsonBool(statusText, "publish_allowed");
    if (publishAllowed.value_or(false)) {
        result.reason = "staging status unexpectedly allows direct publish";
        addWarning(result, "snc_generated_overlay_publish_unexpected_direct_publish");
        return false;
    }

    const auto publishesOverlay = extractJsonBool(statusText, "publishes_overlay");
    if (publishesOverlay.value_or(false)) {
        result.reason = "staging status already claims overlay publish";
        addWarning(result, "snc_generated_overlay_publish_unexpected_publishes_overlay");
        return false;
    }

    const auto stagedDirectory = common::extractJsonString(statusText, "staged_overlay_directory");
    if (!stagedDirectory.has_value() || stagedDirectory->empty()) {
        result.reason = "staging status has no staged overlay directory";
        addWarning(result, "snc_generated_overlay_publish_missing_staged_directory");
        return false;
    }
    result.stagedOverlayDirectory = *stagedDirectory;

    const auto manifestHash = common::extractJsonString(statusText, "manifest_hash");
    if (!manifestHash.has_value() || manifestHash->empty()) {
        result.reason = "staging status has no manifest hash";
        addWarning(result, "snc_generated_overlay_publish_missing_manifest_hash");
        return false;
    }
    result.sourceManifestHash = *manifestHash;

    return true;
}

} // namespace

SncGeneratedOverlayPublishGateResult SncGeneratedOverlayPublishGate::publish(
    const SncGeneratedOverlayPublishGateRequest& request) const
{
    SncGeneratedOverlayPublishGateResult result;
    result.reason = "not started";
    result.stagingStatusPath = request.stagingStatusPath;
    result.activeOverlayDirectory = request.activeOverlayDirectory;
    result.stellarisRunning = request.stellarisRunning;
    result.ownerApproved = request.ownerApprovalToken == ownerApprovalToken;

    if (request.stagingStatusPath.empty() || request.activeOverlayDirectory.empty()) {
        result.reason = "missing staging status path or active overlay directory";
        addWarning(result, "snc_generated_overlay_publish_missing_path");
        return result;
    }

    if (!result.ownerApproved) {
        result.reason = "owner approval token is required";
        addWarning(result, "snc_generated_overlay_publish_missing_owner_approval");
        return result;
    }

    if (isDangerousDirectoryTarget(request.activeOverlayDirectory)) {
        result.reason = "unsafe active generated overlay directory";
        addWarning(result, "snc_generated_overlay_publish_unsafe_active_directory");
        return result;
    }

    if (request.stellarisRunning) {
        result.reason = "Stellaris is running; SNC generated overlay publish deferred";
        addWarning(result, "snc_generated_overlay_publish_stellaris_running");
        return result;
    }

    std::string statusText;
    if (!common::tryReadTextFile(request.stagingStatusPath, statusText)) {
        result.reason = "failed to read staging status";
        addWarning(result, "snc_generated_overlay_publish_status_read_failed");
        return result;
    }

    if (!validateStagingStatus(statusText, result)) {
        return result;
    }

    std::error_code stageError;
    if (!std::filesystem::is_directory(result.stagedOverlayDirectory, stageError) || stageError) {
        result.reason = "staged generated overlay directory does not exist";
        addWarning(result, "snc_generated_overlay_publish_staged_directory_missing");
        return result;
    }

    if (samePath(result.stagedOverlayDirectory, request.activeOverlayDirectory)) {
        result.reason = "staged and active generated overlay directories must be different";
        addWarning(result, "snc_generated_overlay_publish_same_directory");
        return result;
    }

    const auto backupRoot = request.backupRootDirectory.empty()
        ? defaultBackupRootFor(request.activeOverlayDirectory)
        : request.backupRootDirectory;
    std::string backupReason;
    if (!backupActiveDirectory(
            request.activeOverlayDirectory,
            backupRoot,
            result.backupDirectory,
            backupReason)) {
        result.reason = backupReason;
        addWarning(result, "snc_generated_overlay_publish_backup_failed");
        return result;
    }
    result.backupCreated = !result.backupDirectory.empty();

    const generated_overlay::GeneratedOverlayPublisher publisher;
    const auto publishResult = publisher.publish(
        result.stagedOverlayDirectory,
        request.activeOverlayDirectory,
        request.stellarisRunning);
    result.activeOverlayDirectory = publishResult.activeDirectory;
    result.sourceManifestHash = publishResult.sourceManifestHash.empty()
        ? result.sourceManifestHash
        : publishResult.sourceManifestHash;
    result.publishedManifestHash = publishResult.publishedManifestHash;
    result.publishedFileCount = publishResult.files.size();
    if (!publishResult.ok) {
        result.reason = publishResult.reason;
        addWarning(result, "snc_generated_overlay_publish_publisher_failed");
        return result;
    }

    result.ok = true;
    result.published = true;
    result.reason = "owner approved generated overlay published";
    result.readiness = "published";
    return result;
}

std::string serializeSncGeneratedOverlayPublishGateResult(
    const SncGeneratedOverlayPublishGateResult& result)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"ok\": " << (result.ok ? "true" : "false") << ",\n";
    json << "  \"reason\": \"" << jsonEscape(result.reason) << "\",\n";
    json << "  \"readiness\": \"" << jsonEscape(result.readiness) << "\",\n";
    json << "  \"owner_approved\": " << (result.ownerApproved ? "true" : "false") << ",\n";
    json << "  \"published\": " << (result.published ? "true" : "false") << ",\n";
    json << "  \"backup_created\": " << (result.backupCreated ? "true" : "false") << ",\n";
    json << "  \"stellaris_running\": " << (result.stellarisRunning ? "true" : "false") << ",\n";
    json << "  \"staging_status_path\": \"" << jsonEscape(result.stagingStatusPath.generic_string()) << "\",\n";
    json << "  \"staged_overlay_directory\": \"" << jsonEscape(result.stagedOverlayDirectory.generic_string()) << "\",\n";
    json << "  \"active_overlay_directory\": \"" << jsonEscape(result.activeOverlayDirectory.generic_string()) << "\",\n";
    json << "  \"backup_directory\": \"" << jsonEscape(result.backupDirectory.generic_string()) << "\",\n";
    json << "  \"source_manifest_hash\": \"" << jsonEscape(result.sourceManifestHash) << "\",\n";
    json << "  \"published_manifest_hash\": \"" << jsonEscape(result.publishedManifestHash) << "\",\n";
    json << "  \"published_file_count\": " << result.publishedFileCount << ",\n";
    json << "  \"warning_codes\": ";
    writeStringArray(json, result.warningCodes, 2);
    json << ",\n";
    json << "  \"validation_errors\": ";
    writeStringArray(json, result.validationErrors, 2);
    json << "\n";
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus
