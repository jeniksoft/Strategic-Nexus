// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "MpOverlayPackage.h"

#include "ManifestVerifier.h"
#include "common/FileUtil.h"
#include "common/JsonSanity.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace strategic_nexus::generated_overlay {
namespace {

#pragma comment(lib, "bcrypt.lib")

constexpr const char* generatedManifestFileName = "strategic_nexus_generated_manifest.json";
constexpr const char* packageManifestFileName = "strategic_nexus_mp_overlay_package_manifest.json";
constexpr const char* gameplayAffecting = "gameplay_affecting";
constexpr const char* localOnlyDiagnostic = "local_only_diagnostic";

struct PackageIdentity {
    std::string campaignId;
    std::string overlayVersion;
    std::string gameVersion;
    std::string strategicNexusModVersion;
    std::string packageManifestHash;
};

struct ZipFileEntry {
    std::string relativePath;
    std::vector<unsigned char> content;
    std::uint32_t crc32 = 0;
    std::uint32_t localHeaderOffset = 0;
};

constexpr std::uint16_t kZipDosTime = 0;
constexpr std::uint16_t kZipDosDate = 33; // 1980-01-01

void appendLittleEndian16(std::vector<unsigned char>& bytes, const std::uint16_t value)
{
    bytes.push_back(static_cast<unsigned char>(value & 0xffu));
    bytes.push_back(static_cast<unsigned char>((value >> 8u) & 0xffu));
}

void appendLittleEndian32(std::vector<unsigned char>& bytes, const std::uint32_t value)
{
    bytes.push_back(static_cast<unsigned char>(value & 0xffu));
    bytes.push_back(static_cast<unsigned char>((value >> 8u) & 0xffu));
    bytes.push_back(static_cast<unsigned char>((value >> 16u) & 0xffu));
    bytes.push_back(static_cast<unsigned char>((value >> 24u) & 0xffu));
}

bool tryReadBinaryFile(const std::filesystem::path& path, std::vector<unsigned char>& output)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }

    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size < 0) {
        return false;
    }
    input.seekg(0, std::ios::beg);

    output.resize(static_cast<std::size_t>(size));
    if (!output.empty()) {
        input.read(reinterpret_cast<char*>(output.data()), static_cast<std::streamsize>(output.size()));
        if (!input) {
            return false;
        }
    }
    return true;
}

std::uint32_t crc32(const std::vector<unsigned char>& bytes)
{
    std::uint32_t crc = 0xffffffffu;
    for (const unsigned char value : bytes) {
        crc ^= value;
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1u) ^ (0xedb88320u & mask);
        }
    }
    return crc ^ 0xffffffffu;
}

std::optional<std::string> sha256Hex(const std::vector<unsigned char>& bytes)
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectLength = 0;
    DWORD hashLength = 0;
    DWORD bytesWritten = 0;

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        return std::nullopt;
    }

    std::optional<std::string> result;
    std::vector<unsigned char> hashObject;
    std::vector<unsigned char> hashBytes;

    if (BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectLength),
            sizeof(objectLength),
            &bytesWritten,
            0) == 0 &&
        BCryptGetProperty(
            algorithm,
            BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(&hashLength),
            sizeof(hashLength),
            &bytesWritten,
            0) == 0) {
        hashObject.resize(objectLength);
        hashBytes.resize(hashLength);
        if (BCryptCreateHash(algorithm, &hash, hashObject.data(), objectLength, nullptr, 0, 0) == 0 &&
            BCryptHashData(hash, const_cast<PUCHAR>(bytes.data()), static_cast<ULONG>(bytes.size()), 0) == 0 &&
            BCryptFinishHash(hash, hashBytes.data(), hashLength, 0) == 0) {
            static constexpr char kHex[] = "0123456789abcdef";
            std::string text;
            text.reserve(hashBytes.size() * 2);
            for (const unsigned char byte : hashBytes) {
                text.push_back(kHex[(byte >> 4u) & 0x0fu]);
                text.push_back(kHex[byte & 0x0fu]);
            }
            result = text;
        }
    }

    if (hash != nullptr) {
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return result;
}

std::filesystem::path zipPathOrDefault(
    const std::filesystem::path& packageDirectory,
    const std::filesystem::path& outputZipPath)
{
    if (!outputZipPath.empty()) {
        return outputZipPath;
    }
    return packageDirectory.parent_path() / "snc_mp_overlay_package.zip";
}

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

std::vector<std::string> extractStringArrayFromObject(const std::string& text, const char* key)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*\\[([\\s\\S]*?)\\]");
    std::smatch match;
    if (!std::regex_search(text, match, pattern)) {
        return {};
    }

    std::vector<std::string> values;
    const std::regex valuePattern("\"([^\"]+)\"");
    auto begin = std::sregex_iterator(match[1].first, match[1].second, valuePattern);
    const auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        values.push_back((*it)[1].str());
    }
    return values;
}

std::size_t countBootstrapCampaignSignals(const std::string& manifestText)
{
    const std::regex bootstrapSeedPattern("\"bootstrap_rotation_seed_id\"\\s*:");
    return static_cast<std::size_t>(std::distance(
        std::sregex_iterator(manifestText.begin(), manifestText.end(), bootstrapSeedPattern),
        std::sregex_iterator()));
}

std::string buildCopyableStatusText(
    const std::string& campaignId,
    const std::string& overlayVersion,
    const std::string& gameVersion,
    const std::string& strategicNexusModVersion,
    const std::string& handoffStatus,
    const std::string& humanControlGuardState,
    const std::string& readiness,
    const std::string& packageManifestHash,
    bool previousHostAvailable,
    const std::filesystem::path& packageDirectory,
    const std::string& provenanceState,
    const std::vector<std::string>& sourceQualities,
    const std::size_t bootstrapCampaignCount)
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
    text << "provenance_state: " << provenanceState << "\n";
    text << "human_control_guard_state: " << humanControlGuardState << "\n";
    text << "source_quality_count: " << sourceQualities.size() << "\n";
    for (const auto& sourceQuality : sourceQualities) {
        text << "source_quality: " << sourceQuality << "\n";
    }
    text << "bootstrap_campaign_count: " << bootstrapCampaignCount << "\n";
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

std::string buildFailureStatusText(
    const std::string& warningCode,
    const std::string& nextStep,
    const std::filesystem::path& packageDirectory,
    const std::optional<PackageIdentity>& identity = std::nullopt)
{
    std::ostringstream text;
    text << "readiness: not_ready\n";
    text << "warning_code: " << warningCode << "\n";
    text << "next_step: " << nextStep << "\n";
    text << "verify_command: Strategic Nexus.exe --verify-mp-overlay-package \"" << packageDirectory.generic_string() << "\"\n";
    text << "import_command: Strategic Nexus.exe --import-mp-overlay-package \"" << packageDirectory.generic_string()
         << "\" <target_overlay_dir>\n";
    if (identity.has_value()) {
        text << "strict_verify_command: Strategic Nexus.exe --verify-mp-overlay-package \"" << packageDirectory.generic_string()
             << "\" " << identity->campaignId << " " << identity->overlayVersion << " " << identity->gameVersion << " "
             << identity->strategicNexusModVersion << " " << identity->packageManifestHash << "\n";
        text << "strict_import_command: Strategic Nexus.exe --import-mp-overlay-package \"" << packageDirectory.generic_string()
             << "\" <target_overlay_dir> " << identity->campaignId << " " << identity->overlayVersion << " "
             << identity->gameVersion << " " << identity->strategicNexusModVersion << " " << identity->packageManifestHash
             << "\n";
    }
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
    const std::string& humanControlGuardState,
    const std::vector<std::string>& sourceQualities,
    const std::size_t bootstrapCampaignCount,
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
    manifest << "  \"human_control_guard_state\": \"" << jsonEscape(humanControlGuardState) << "\",\n";
    manifest << "  \"source_qualities\": [";
    for (std::size_t index = 0; index < sourceQualities.size(); ++index) {
        if (index > 0) {
            manifest << ", ";
        }
        manifest << "\"" << jsonEscape(sourceQualities[index]) << "\"";
    }
    manifest << "],\n";
    manifest << "  \"bootstrap_campaign_count\": " << bootstrapCampaignCount << ",\n";
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
    const auto sourceQualities = extractStringArrayFromObject(generatedManifestText, "source_qualities");
    if (sourceQualities.empty()) {
        result.reason = "source overlay manifest missing source provenance";
        return result;
    }
    const auto bootstrapCampaignCount = countBootstrapCampaignSignals(generatedManifestText);
    constexpr const char* humanControlGuardState = "runtime_is_ai_yes";

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
        humanControlGuardState,
        sourceQualities,
        bootstrapCampaignCount,
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

MpOverlayPackageZipExportResult MpOverlayPackageExporter::exportPackageZip(
    const std::filesystem::path& packageDirectory,
    const std::filesystem::path& outputZipPath) const
{
    MpOverlayPackageZipExportResult result;
    const auto effectiveZipPath = zipPathOrDefault(packageDirectory, outputZipPath);
    result.packageZipPath = effectiveZipPath;

    if (packageDirectory.empty()) {
        result.reason = "missing package directory";
        return result;
    }
    if (effectiveZipPath.empty()) {
        result.reason = "missing zip output path";
        return result;
    }

    const MpOverlayPackageVerifier verifier;
    const auto verification = verifier.verify(packageDirectory);
    if (!verification.ok) {
        result.reason = "package verification failed: " + verification.reason;
        return result;
    }

    std::vector<ZipFileEntry> entries;
    entries.reserve(verification.files.size());
    for (const auto& file : verification.files) {
        ZipFileEntry entry;
        entry.relativePath = file.path;
        if (!tryReadBinaryFile(packageDirectory / std::filesystem::path(file.path), entry.content)) {
            result.reason = "failed to read package file for zip export";
            return result;
        }
        entry.crc32 = crc32(entry.content);
        entries.push_back(std::move(entry));
    }

    std::sort(
        entries.begin(),
        entries.end(),
        [](const ZipFileEntry& left, const ZipFileEntry& right) { return left.relativePath < right.relativePath; });

    std::vector<unsigned char> zipBytes;
    for (auto& entry : entries) {
        const auto pathLength = static_cast<std::uint16_t>(entry.relativePath.size());
        if (pathLength != entry.relativePath.size() || entry.content.size() > 0xffffffffu) {
            result.reason = "package zip export input exceeds v0 zip bounds";
            return result;
        }

        entry.localHeaderOffset = static_cast<std::uint32_t>(zipBytes.size());
        appendLittleEndian32(zipBytes, 0x04034b50u);
        appendLittleEndian16(zipBytes, 20u);
        appendLittleEndian16(zipBytes, 0u);
        appendLittleEndian16(zipBytes, 0u);
        appendLittleEndian16(zipBytes, kZipDosTime);
        appendLittleEndian16(zipBytes, kZipDosDate);
        appendLittleEndian32(zipBytes, entry.crc32);
        appendLittleEndian32(zipBytes, static_cast<std::uint32_t>(entry.content.size()));
        appendLittleEndian32(zipBytes, static_cast<std::uint32_t>(entry.content.size()));
        appendLittleEndian16(zipBytes, pathLength);
        appendLittleEndian16(zipBytes, 0u);
        zipBytes.insert(zipBytes.end(), entry.relativePath.begin(), entry.relativePath.end());
        zipBytes.insert(zipBytes.end(), entry.content.begin(), entry.content.end());
    }

    const std::uint32_t centralDirectoryOffset = static_cast<std::uint32_t>(zipBytes.size());
    for (const auto& entry : entries) {
        const auto pathLength = static_cast<std::uint16_t>(entry.relativePath.size());
        appendLittleEndian32(zipBytes, 0x02014b50u);
        appendLittleEndian16(zipBytes, 20u);
        appendLittleEndian16(zipBytes, 20u);
        appendLittleEndian16(zipBytes, 0u);
        appendLittleEndian16(zipBytes, 0u);
        appendLittleEndian16(zipBytes, kZipDosTime);
        appendLittleEndian16(zipBytes, kZipDosDate);
        appendLittleEndian32(zipBytes, entry.crc32);
        appendLittleEndian32(zipBytes, static_cast<std::uint32_t>(entry.content.size()));
        appendLittleEndian32(zipBytes, static_cast<std::uint32_t>(entry.content.size()));
        appendLittleEndian16(zipBytes, pathLength);
        appendLittleEndian16(zipBytes, 0u);
        appendLittleEndian16(zipBytes, 0u);
        appendLittleEndian16(zipBytes, 0u);
        appendLittleEndian16(zipBytes, 0u);
        appendLittleEndian32(zipBytes, 0u);
        appendLittleEndian32(zipBytes, entry.localHeaderOffset);
        zipBytes.insert(zipBytes.end(), entry.relativePath.begin(), entry.relativePath.end());
    }

    const std::uint32_t centralDirectorySize = static_cast<std::uint32_t>(zipBytes.size()) - centralDirectoryOffset;
    appendLittleEndian32(zipBytes, 0x06054b50u);
    appendLittleEndian16(zipBytes, 0u);
    appendLittleEndian16(zipBytes, 0u);
    appendLittleEndian16(zipBytes, static_cast<std::uint16_t>(entries.size()));
    appendLittleEndian16(zipBytes, static_cast<std::uint16_t>(entries.size()));
    appendLittleEndian32(zipBytes, centralDirectorySize);
    appendLittleEndian32(zipBytes, centralDirectoryOffset);
    appendLittleEndian16(zipBytes, 0u);

    std::error_code directoryError;
    if (!effectiveZipPath.parent_path().empty()) {
        std::filesystem::create_directories(effectiveZipPath.parent_path(), directoryError);
        if (directoryError) {
            result.reason = "failed to create zip output directory";
            return result;
        }
    }

    {
        std::ofstream output(effectiveZipPath, std::ios::binary | std::ios::trunc);
        if (!output) {
            result.reason = "failed to open zip output path";
            return result;
        }
        output.write(reinterpret_cast<const char*>(zipBytes.data()), static_cast<std::streamsize>(zipBytes.size()));
        if (!output) {
            result.reason = "failed to write zip output";
            return result;
        }
    }

    const auto hash = sha256Hex(zipBytes);
    if (!hash.has_value()) {
        result.reason = "failed to hash exported zip";
        return result;
    }

    result.ok = true;
    result.reason = "zip_exported";
    result.packageZipHash = *hash;
    result.packageZipBytes = zipBytes.size();
    return result;
}

MpOverlayPackageVerificationResult MpOverlayPackageVerifier::verify(const std::filesystem::path& packageDirectory) const
{
    MpOverlayPackageVerificationResult result;
    if (packageDirectory.empty()) {
        result.reason = "missing package directory";
        result.statusText = buildFailureStatusText(
            "mp_overlay_package_directory_missing",
            "provide an existing package directory and rerun verify",
            packageDirectory);
        return result;
    }

    const auto manifestPath = packageDirectory / packageManifestFileName;
    if (!std::filesystem::exists(manifestPath)) {
        result.reason = "missing MP overlay package manifest";
        result.statusText = buildFailureStatusText(
            "mp_overlay_package_manifest_missing",
            "re-export package on host before import/verify",
            packageDirectory);
        return result;
    }

    const std::string manifestText = common::readTextFile(manifestPath);
    result.packageManifestHash = fnv1a64Hex(manifestText);
    if (!common::hasBalancedJsonDelimiters(manifestText)) {
        result.reason = "malformed MP overlay package manifest";
        result.statusText = buildFailureStatusText(
            "mp_overlay_package_manifest_malformed",
            "re-export package on host before import/verify",
            packageDirectory);
        return result;
    }

    const auto campaignId = extractStringFromObject(manifestText, "campaign_id");
    const auto overlayVersion = extractStringFromObject(manifestText, "overlay_version");
    const auto gameVersion = extractStringFromObject(manifestText, "game_version");
    const auto strategicNexusModVersion = extractStringFromObject(manifestText, "strategic_nexus_mod_version");
    const auto handoffStatus = extractStringFromObject(manifestText, "handoff_status");
    const auto humanControlGuardState = extractStringFromObject(manifestText, "human_control_guard_state");
    const auto previousHostAvailable = extractBoolFromObject(manifestText, "previous_host_available");

    if (!campaignId.has_value() || !overlayVersion.has_value() || !gameVersion.has_value() ||
        !strategicNexusModVersion.has_value() || !handoffStatus.has_value() || !humanControlGuardState.has_value() ||
        !previousHostAvailable.has_value()) {
        result.reason = "malformed MP overlay package manifest";
        result.statusText = buildFailureStatusText(
            "mp_overlay_package_manifest_malformed",
            "re-export package on host before import/verify",
            packageDirectory);
        return result;
    }
    if (campaignId->empty() || overlayVersion->empty() || gameVersion->empty() || strategicNexusModVersion->empty() ||
        handoffStatus->empty()) {
        result.reason = "malformed MP overlay package manifest";
        result.statusText = buildFailureStatusText(
            "mp_overlay_package_manifest_malformed",
            "re-export package on host before import/verify",
            packageDirectory);
        return result;
    }
    if (!isSafePackageMetadataValue(*campaignId) || !isSafePackageMetadataValue(*overlayVersion) ||
        !isSafePackageMetadataValue(*gameVersion) || !isSafePackageMetadataValue(*strategicNexusModVersion)) {
        result.reason = "malformed MP overlay package manifest";
        result.statusText = buildFailureStatusText(
            "mp_overlay_package_manifest_malformed",
            "re-export package on host before import/verify",
            packageDirectory);
        return result;
    }
    const std::string expectedHandoff = *previousHostAvailable ? "complete" : "degraded_previous_host_unavailable";
    if (*handoffStatus != expectedHandoff) {
        result.reason = "malformed MP overlay package manifest";
        result.statusText = buildFailureStatusText(
            "mp_overlay_package_manifest_malformed",
            "re-export package on host before import/verify",
            packageDirectory);
        return result;
    }
    if (*humanControlGuardState != "runtime_is_ai_yes") {
        result.reason = "malformed MP overlay package manifest";
        result.statusText = buildFailureStatusText(
            "mp_overlay_package_manifest_malformed",
            "re-export package on host before import/verify",
            packageDirectory);
        return result;
    }

    result.campaignId = *campaignId;
    result.overlayVersion = *overlayVersion;
    result.gameVersion = *gameVersion;
    result.strategicNexusModVersion = *strategicNexusModVersion;
    result.handoffStatus = *handoffStatus;
    result.humanControlGuardState = *humanControlGuardState;
    result.sourceQualities = extractStringArrayFromObject(manifestText, "source_qualities");
    const auto bootstrapCampaignCount = extractSizeFromObject(manifestText, "bootstrap_campaign_count");
    result.bootstrapCampaignCount = bootstrapCampaignCount.value_or(0);
    result.provenanceState =
        bootstrapCampaignCount.has_value() ? "present" : (result.sourceQualities.empty() ? "missing" : "malformed");
    const PackageIdentity identity{
        result.campaignId,
        result.overlayVersion,
        result.gameVersion,
        result.strategicNexusModVersion,
        result.packageManifestHash
    };

    const auto objects = extractFileObjects(manifestText);
    if (objects.empty()) {
        result.reason = "MP overlay package manifest contains no files";
        result.statusText = buildFailureStatusText(
            "mp_overlay_package_manifest_files_missing",
            "re-export package on host before import/verify",
            packageDirectory,
            identity);
        return result;
    }

    bool allOk = true;
    std::unordered_set<std::string> expectedFiles;
    expectedFiles.insert(packageManifestFileName);
    for (const auto& object : objects) {
        const auto parsed = parseFileEntry(object);
        if (!parsed.has_value()) {
            result.reason = "MP overlay package manifest contains invalid file entry";
            result.statusText = buildFailureStatusText(
                "mp_overlay_package_manifest_invalid_file_entry",
                "re-export package on host before import/verify",
                packageDirectory,
                identity);
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
            result.humanControlGuardState,
            result.readiness,
            result.packageManifestHash,
            *previousHostAvailable,
            packageDirectory,
            result.provenanceState,
            result.sourceQualities,
            result.bootstrapCampaignCount);
        result.reason = "accepted";
    } else if (unexpected) {
        result.readiness = "not_ready";
        result.reason = "MP overlay package contains unexpected files";
        result.statusText = buildFailureStatusText(
            "mp_overlay_package_unexpected_files",
            "remove unexpected files or re-export package on host",
            packageDirectory,
            identity);
    } else {
        result.readiness = "not_ready";
        result.reason = "MP overlay package files do not match manifest";
        result.statusText = buildFailureStatusText(
            "mp_overlay_package_files_mismatch_manifest",
            "re-export package on host and import+verify again",
            packageDirectory,
            identity);
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
    result.humanControlGuardState = packageVerification.humanControlGuardState;
    result.packageManifestHash = packageVerification.packageManifestHash;
    result.provenanceState = packageVerification.provenanceState;
    result.sourceQualities = packageVerification.sourceQualities;
    result.bootstrapCampaignCount = packageVerification.bootstrapCampaignCount;
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
