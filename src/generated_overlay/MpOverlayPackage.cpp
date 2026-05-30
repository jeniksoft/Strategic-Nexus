// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "MpOverlayPackage.h"

#include "ManifestVerifier.h"
#include "common/FileUtil.h"
#include "common/JsonSanity.h"

#include <filesystem>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace strategic_nexus::generated_overlay {
namespace {

constexpr const char* generatedManifestFileName = "strategic_nexus_generated_manifest.json";
constexpr const char* packageManifestFileName = "strategic_nexus_mp_overlay_package_manifest.json";
constexpr const char* gameplayAffecting = "gameplay_affecting";
constexpr const char* localOnlyDiagnostic = "local_only_diagnostic";

std::string jsonEscape(const std::string& value)
{
    std::ostringstream output;
    for (const char ch : value) {
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
        default:
            output << ch;
            break;
        }
    }
    return output.str();
}

std::optional<std::string> extractStringFromObject(const std::string& objectText, const char* key)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*\"((?:\\\\.|[^\"\\\\])*)\"");
    std::smatch match;
    if (!std::regex_search(objectText, match, pattern)) {
        return std::nullopt;
    }
    return match[1].str();
}

std::optional<std::size_t> extractSizeFromObject(const std::string& objectText, const char* key)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (!std::regex_search(objectText, match, pattern)) {
        return std::nullopt;
    }

    try {
        return static_cast<std::size_t>(std::stoull(match[1].str()));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<bool> extractBoolFromObject(const std::string& objectText, const char* key)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(objectText, match, pattern)) {
        return std::nullopt;
    }
    return match[1].str() == "true";
}

std::vector<std::string> extractFileObjects(const std::string& text)
{
    std::vector<std::string> objects;
    const std::regex filesStartPattern("\"files\"\\s*:\\s*\\[");
    std::smatch match;
    if (!std::regex_search(text, match, filesStartPattern)) {
        return objects;
    }

    const std::size_t start = static_cast<std::size_t>(match.position()) + match.length();
    int arrayDepth = 1;
    int objectDepth = 0;
    bool inString = false;
    bool escaped = false;
    std::size_t objectStart = std::string::npos;

    for (std::size_t index = start; index < text.size(); ++index) {
        const char ch = text[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
        } else if (ch == '[') {
            ++arrayDepth;
        } else if (ch == ']') {
            --arrayDepth;
            if (arrayDepth == 0) {
                break;
            }
        } else if (ch == '{') {
            if (objectDepth == 0) {
                objectStart = index;
            }
            ++objectDepth;
        } else if (ch == '}') {
            --objectDepth;
            if (objectDepth == 0 && objectStart != std::string::npos) {
                objects.push_back(text.substr(objectStart, index - objectStart + 1));
                objectStart = std::string::npos;
            }
        }
    }

    return objects;
}

std::string buildCopyableStatusText(
    const std::string& campaignId,
    const std::string& overlayVersion,
    const std::string& gameVersion,
    const std::string& strategicNexusModVersion,
    const std::string& handoffStatus,
    const std::string& readiness,
    const std::string& packageManifestHash,
    bool previousHostAvailable,
    const std::filesystem::path& packageDirectory)
{
    const bool handoffComplete = handoffStatus == "complete" && previousHostAvailable;
    const bool handoffDegraded = handoffStatus == "degraded_previous_host_unavailable" && !previousHostAvailable;

    std::ostringstream text;
    text << "Strategic Nexus MP overlay package\n";
    text << "campaign_id: " << campaignId << "\n";
    text << "overlay_version: " << overlayVersion << "\n";
    text << "game_version: " << gameVersion << "\n";
    text << "strategic_nexus_mod_version: " << strategicNexusModVersion << "\n";
    text << "handoff_status: " << handoffStatus << "\n";
    text << "readiness: " << readiness << "\n";
    text << "package_manifest_hash: " << packageManifestHash << "\n";
    text << "previous_host_available: " << (previousHostAvailable ? "true" : "false") << "\n";
    text << "host_readiness: " << readiness << "\n";
    text << "host_next_step: share this package and package_manifest_hash with every joining player\n";
    text << "client_readiness_gate: import_and_verify_before_join\n";
    text << "client_next_step: import package, verify package_manifest_hash, then join lobby\n";
    text << "verify_command: Strategic Nexus.exe --verify-mp-overlay-package \"" << packageDirectory.generic_string() << "\"\n";
    text << "import_command: Strategic Nexus.exe --import-mp-overlay-package \"" << packageDirectory.generic_string()
         << "\" <target_overlay_dir>\n";
    text << "strict_verify_command: Strategic Nexus.exe --verify-mp-overlay-package \"" << packageDirectory.generic_string()
         << "\" " << campaignId << " " << overlayVersion << " " << gameVersion << " " << strategicNexusModVersion << " "
         << packageManifestHash << "\n";
    text << "strict_import_command: Strategic Nexus.exe --import-mp-overlay-package \"" << packageDirectory.generic_string()
         << "\" <target_overlay_dir> " << campaignId << " " << overlayVersion << " " << gameVersion << " "
         << strategicNexusModVersion << " " << packageManifestHash << "\n";
    if (handoffComplete) {
        text << "host_handoff_state: previous host package continuity available\n";
    } else if (handoffDegraded) {
        text << "host_handoff_state: previous host unavailable; manual save recovery may be needed\n";
    } else {
        text << "host_handoff_state: unknown\n";
    }
    text << "mp_join_check: every player must use this same package_manifest_hash before joining\n";
    return text.str();
}

bool isSafePackageMetadataValue(const std::string& value)
{
    if (value.empty() || value.size() > 96) {
        return false;
    }

    for (const unsigned char ch : value) {
        if (std::isalnum(ch) || ch == '_' || ch == '-' || ch == '.') {
            continue;
        }
        return false;
    }

    return true;
}

bool isSafePackageRelativePath(const std::string& relativePath)
{
    if (relativePath.empty() || relativePath.find('\\') != std::string::npos) {
        return false;
    }

    const std::filesystem::path path(relativePath);
    if (path.is_absolute()) {
        return false;
    }

    for (const auto& part : path) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

std::optional<MpOverlayPackageFileVerification> parseFileEntry(const std::string& objectText)
{
    const auto path = extractStringFromObject(objectText, "path");
    const auto checksumRelevance = extractStringFromObject(objectText, "checksum_relevance");
    const auto hash = extractStringFromObject(objectText, "hash");
    const auto byteCount = extractSizeFromObject(objectText, "byte_count");
    if (!path.has_value() || !checksumRelevance.has_value() || !hash.has_value() || !byteCount.has_value()) {
        return std::nullopt;
    }
    if (!isSafePackageRelativePath(*path)) {
        return std::nullopt;
    }
    if (*checksumRelevance != gameplayAffecting && *checksumRelevance != localOnlyDiagnostic) {
        return std::nullopt;
    }

    MpOverlayPackageFileVerification file;
    file.path = *path;
    file.checksumRelevance = *checksumRelevance;
    file.expectedHash = *hash;
    file.expectedByteCount = *byteCount;
    return file;
}

std::string buildPackageManifest(
    const std::string& campaignId,
    const std::string& overlayVersion,
    const std::string& gameVersion,
    const std::string& strategicNexusModVersion,
    bool previousHostAvailable,
    const std::vector<MpOverlayPackageFileVerification>& files)
{
    std::ostringstream manifest;
    manifest << "{\n";
    manifest << "  \"schema_version\": 1,\n";
    manifest << "  \"package_kind\": \"strategic_nexus_mp_generated_overlay_package_v0\",\n";
    manifest << "  \"campaign_id\": \"" << jsonEscape(campaignId) << "\",\n";
    manifest << "  \"overlay_version\": \"" << jsonEscape(overlayVersion) << "\",\n";
    manifest << "  \"game_version\": \"" << jsonEscape(gameVersion) << "\",\n";
    manifest << "  \"strategic_nexus_mod_version\": \"" << jsonEscape(strategicNexusModVersion) << "\",\n";
    manifest << "  \"previous_host_available\": " << (previousHostAvailable ? "true" : "false") << ",\n";
    manifest << "  \"handoff_status\": \"" << (previousHostAvailable ? "complete" : "degraded_previous_host_unavailable") << "\",\n";
    manifest << "  \"files\": [\n";
    for (std::size_t index = 0; index < files.size(); ++index) {
        const auto& file = files[index];
        manifest << "    {\n";
        manifest << "      \"path\": \"" << jsonEscape(file.path) << "\",\n";
        manifest << "      \"checksum_relevance\": \"" << jsonEscape(file.checksumRelevance) << "\",\n";
        manifest << "      \"hash_algorithm\": \"fnv1a64\",\n";
        manifest << "      \"hash\": \"" << jsonEscape(file.expectedHash) << "\",\n";
        manifest << "      \"byte_count\": " << file.expectedByteCount << "\n";
        manifest << "    }" << (index + 1 < files.size() ? "," : "") << "\n";
    }
    manifest << "  ]\n";
    manifest << "}\n";
    return manifest.str();
}

bool removeDirectoryIfExists(const std::filesystem::path& path)
{
    if (path.empty()) {
        return false;
    }
    std::error_code removeError;
    std::filesystem::remove_all(path, removeError);
    return !removeError;
}

} // namespace

MpOverlayPackageExportResult MpOverlayPackageExporter::exportPackage(
    const std::filesystem::path& sourceOverlayDirectory,
    const std::string& campaignId,
    const std::string& overlayVersion,
    const std::string& gameVersion,
    const std::string& strategicNexusModVersion,
    const std::filesystem::path& outputPackageDirectory,
    bool previousHostAvailable) const
{
    MpOverlayPackageExportResult result;
    if (sourceOverlayDirectory.empty() || outputPackageDirectory.empty() || campaignId.empty() || overlayVersion.empty() ||
        gameVersion.empty() || strategicNexusModVersion.empty()) {
        result.reason = "missing package export input";
        return result;
    }
    if (!isSafePackageMetadataValue(campaignId) || !isSafePackageMetadataValue(overlayVersion) ||
        !isSafePackageMetadataValue(gameVersion) || !isSafePackageMetadataValue(strategicNexusModVersion)) {
        result.reason = "invalid package export input";
        return result;
    }

    const ManifestVerifier sourceVerifier;
    const auto sourceResult = sourceVerifier.verify(sourceOverlayDirectory);
    if (!sourceResult.ok) {
        result.reason = "source overlay verification failed: " + sourceResult.reason;
        return result;
    }

    if (!removeDirectoryIfExists(outputPackageDirectory)) {
        result.reason = "failed to clean output package directory";
        return result;
    }

    std::vector<MpOverlayPackageFileVerification> files;
    const auto generatedManifestPath = sourceOverlayDirectory / generatedManifestFileName;
    std::string generatedManifestText;
    if (!common::tryReadTextFile(generatedManifestPath, generatedManifestText)) {
        result.reason = "failed to read generated overlay manifest";
        return result;
    }

    MpOverlayPackageFileVerification generatedManifestEntry;
    generatedManifestEntry.path = generatedManifestFileName;
    generatedManifestEntry.checksumRelevance = localOnlyDiagnostic;
    generatedManifestEntry.expectedHash = fnv1a64Hex(generatedManifestText);
    generatedManifestEntry.actualHash = generatedManifestEntry.expectedHash;
    generatedManifestEntry.expectedByteCount = generatedManifestText.size();
    generatedManifestEntry.actualByteCount = generatedManifestEntry.expectedByteCount;
    generatedManifestEntry.exists = true;
    generatedManifestEntry.hashMatches = true;
    generatedManifestEntry.byteCountMatches = true;
    files.push_back(generatedManifestEntry);

    for (const auto& file : sourceResult.files) {
        MpOverlayPackageFileVerification entry;
        entry.path = file.path;
        entry.checksumRelevance = file.checksumRelevance;
        entry.expectedHash = file.actualHash;
        entry.actualHash = file.actualHash;
        entry.expectedByteCount = file.actualByteCount;
        entry.actualByteCount = file.actualByteCount;
        entry.exists = true;
        entry.hashMatches = true;
        entry.byteCountMatches = true;
        files.push_back(entry);
    }

    for (const auto& file : files) {
        const auto sourcePath = sourceOverlayDirectory / std::filesystem::path(file.path);
        const auto destinationPath = outputPackageDirectory / std::filesystem::path(file.path);
        std::string text;
        if (!common::tryReadTextFile(sourcePath, text)) {
            result.reason = "failed to read package source file";
            return result;
        }
        if (!common::writeTextFileAtomically(destinationPath, text)) {
            result.reason = "failed to copy package file";
            return result;
        }
    }

    const auto packageManifestText = buildPackageManifest(
        campaignId,
        overlayVersion,
        gameVersion,
        strategicNexusModVersion,
        previousHostAvailable,
        files);
    if (!common::writeTextFileAtomically(outputPackageDirectory / packageManifestFileName, packageManifestText)) {
        result.reason = "failed to write package manifest";
        return result;
    }

    result.ok = true;
    result.reason = previousHostAvailable ? "accepted" : "accepted_degraded_previous_host_unavailable";
    result.files = files;
    return result;
}

MpOverlayPackageVerificationResult MpOverlayPackageVerifier::verify(const std::filesystem::path& packageDirectory) const
{
    MpOverlayPackageVerificationResult result;
    if (packageDirectory.empty()) {
        result.reason = "missing package directory";
        return result;
    }

    const auto manifestPath = packageDirectory / packageManifestFileName;
    if (!std::filesystem::exists(manifestPath)) {
        result.reason = "missing MP overlay package manifest";
        return result;
    }

    const std::string manifestText = common::readTextFile(manifestPath);
    result.packageManifestHash = fnv1a64Hex(manifestText);
    if (!common::hasBalancedJsonDelimiters(manifestText)) {
        result.reason = "malformed MP overlay package manifest";
        return result;
    }

    const auto campaignId = extractStringFromObject(manifestText, "campaign_id");
    const auto overlayVersion = extractStringFromObject(manifestText, "overlay_version");
    const auto gameVersion = extractStringFromObject(manifestText, "game_version");
    const auto strategicNexusModVersion = extractStringFromObject(manifestText, "strategic_nexus_mod_version");
    const auto handoffStatus = extractStringFromObject(manifestText, "handoff_status");
    const auto previousHostAvailable = extractBoolFromObject(manifestText, "previous_host_available");

    if (!campaignId.has_value() || !overlayVersion.has_value() || !gameVersion.has_value() ||
        !strategicNexusModVersion.has_value() || !handoffStatus.has_value() || !previousHostAvailable.has_value()) {
        result.reason = "malformed MP overlay package manifest";
        return result;
    }
    if (campaignId->empty() || overlayVersion->empty() || gameVersion->empty() || strategicNexusModVersion->empty() ||
        handoffStatus->empty()) {
        result.reason = "malformed MP overlay package manifest";
        return result;
    }
    if (!isSafePackageMetadataValue(*campaignId) || !isSafePackageMetadataValue(*overlayVersion) ||
        !isSafePackageMetadataValue(*gameVersion) || !isSafePackageMetadataValue(*strategicNexusModVersion)) {
        result.reason = "malformed MP overlay package manifest";
        return result;
    }
    const std::string expectedHandoff = *previousHostAvailable ? "complete" : "degraded_previous_host_unavailable";
    if (*handoffStatus != expectedHandoff) {
        result.reason = "malformed MP overlay package manifest";
        return result;
    }

    result.campaignId = *campaignId;
    result.overlayVersion = *overlayVersion;
    result.gameVersion = *gameVersion;
    result.strategicNexusModVersion = *strategicNexusModVersion;
    result.handoffStatus = *handoffStatus;

    const auto objects = extractFileObjects(manifestText);
    if (objects.empty()) {
        result.reason = "MP overlay package manifest contains no files";
        return result;
    }

    bool allOk = true;
    std::unordered_set<std::string> expectedFiles;
    expectedFiles.insert(packageManifestFileName);
    for (const auto& object : objects) {
        const auto parsed = parseFileEntry(object);
        if (!parsed.has_value()) {
            result.reason = "MP overlay package manifest contains invalid file entry";
            return result;
        }

        auto file = *parsed;
        expectedFiles.insert(file.path);
        const auto filePath = packageDirectory / std::filesystem::path(file.path);
        file.exists = std::filesystem::exists(filePath);
        if (file.exists) {
            const std::string text = common::readTextFile(filePath);
            file.actualHash = fnv1a64Hex(text);
            file.actualByteCount = text.size();
            file.hashMatches = file.actualHash == file.expectedHash;
            file.byteCountMatches = file.actualByteCount == file.expectedByteCount;
        }

        allOk = allOk && file.exists && file.hashMatches && file.byteCountMatches;
        result.files.push_back(file);
    }

    std::error_code iterError;
    for (std::filesystem::recursive_directory_iterator it(packageDirectory, iterError), end; it != end; it.increment(iterError)) {
        if (iterError) {
            break;
        }

        std::error_code typeError;
        if (!it->is_regular_file(typeError) || typeError) {
            continue;
        }

        std::error_code relativeError;
        const auto relative = std::filesystem::relative(it->path(), packageDirectory, relativeError);
        if (relativeError) {
            continue;
        }

        const std::string relativePath = relative.generic_string();
        if (expectedFiles.find(relativePath) == expectedFiles.end()) {
            result.unexpectedFiles.push_back(relativePath);
        }
    }

    const bool unexpected = !result.unexpectedFiles.empty();
    result.ok = allOk && !unexpected;
    if (result.ok) {
        result.readiness = "ready_for_mp";
        result.statusText = buildCopyableStatusText(
            result.campaignId,
            result.overlayVersion,
            result.gameVersion,
            result.strategicNexusModVersion,
            result.handoffStatus,
            result.readiness,
            result.packageManifestHash,
            *previousHostAvailable,
            packageDirectory);
        result.reason = "accepted";
    } else if (unexpected) {
        result.readiness = "not_ready";
        result.reason = "MP overlay package contains unexpected files";
    } else {
        result.readiness = "not_ready";
        result.reason = "MP overlay package files do not match manifest";
    }
    return result;
}

MpOverlayPackageImportResult MpOverlayPackageImporter::importPackage(
    const std::filesystem::path& packageDirectory,
    const std::filesystem::path& targetOverlayDirectory) const
{
    MpOverlayPackageImportResult result;
    if (packageDirectory.empty() || targetOverlayDirectory.empty()) {
        result.reason = "missing package import input";
        return result;
    }

    const MpOverlayPackageVerifier packageVerifier;
    const auto packageVerification = packageVerifier.verify(packageDirectory);
    result.campaignId = packageVerification.campaignId;
    result.overlayVersion = packageVerification.overlayVersion;
    result.gameVersion = packageVerification.gameVersion;
    result.strategicNexusModVersion = packageVerification.strategicNexusModVersion;
    result.handoffStatus = packageVerification.handoffStatus;
    result.packageManifestHash = packageVerification.packageManifestHash;
    result.readiness = packageVerification.readiness;
    result.statusText = packageVerification.statusText;
    if (!packageVerification.ok) {
        result.reason = "package verification failed: " + packageVerification.reason;
        return result;
    }

    if (!removeDirectoryIfExists(targetOverlayDirectory)) {
        result.reason = "failed to clean target overlay directory";
        return result;
    }

    for (const auto& file : packageVerification.files) {
        const auto sourcePath = packageDirectory / std::filesystem::path(file.path);
        const auto destinationPath = targetOverlayDirectory / std::filesystem::path(file.path);
        std::string text;
        if (!common::tryReadTextFile(sourcePath, text)) {
            removeDirectoryIfExists(targetOverlayDirectory);
            result.reason = "failed to read package source file";
            return result;
        }
        if (!common::writeTextFileAtomically(destinationPath, text)) {
            removeDirectoryIfExists(targetOverlayDirectory);
            result.reason = "failed to write imported overlay file";
            return result;
        }
        result.importedFiles.push_back(file);
    }

    const ManifestVerifier importedOverlayVerifier;
    const auto importedOverlayVerification = importedOverlayVerifier.verify(targetOverlayDirectory);
    if (!importedOverlayVerification.ok) {
        removeDirectoryIfExists(targetOverlayDirectory);
        result.reason = "imported overlay verification failed: " + importedOverlayVerification.reason;
        result.importedFiles.clear();
        return result;
    }

    result.ok = true;
    result.reason = "accepted";
    return result;
}

} // namespace strategic_nexus::generated_overlay
