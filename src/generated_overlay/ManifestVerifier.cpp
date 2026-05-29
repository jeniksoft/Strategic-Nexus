// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "ManifestVerifier.h"

#include "common/FileUtil.h"
#include "common/JsonSanity.h"

#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <unordered_set>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>

namespace strategic_nexus::generated_overlay {
namespace {

constexpr const char* manifestFileName = "strategic_nexus_generated_manifest.json";
constexpr const char* gameplayAffecting = "gameplay_affecting";

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

std::vector<std::string> extractManifestFileObjects(const std::string& manifestText)
{
    std::vector<std::string> objects;
    const std::regex filesStartPattern("\"files\"\\s*:\\s*\\[");
    std::smatch match;
    if (!std::regex_search(manifestText, match, filesStartPattern)) {
        return objects;
    }

    const std::size_t start = static_cast<std::size_t>(match.position()) + match.length();
    int arrayDepth = 1;
    int objectDepth = 0;
    bool inString = false;
    bool escaped = false;
    std::size_t objectStart = std::string::npos;

    for (std::size_t index = start; index < manifestText.size(); ++index) {
        const char ch = manifestText[index];
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
            continue;
        }
        if (ch == '[') {
            ++arrayDepth;
            continue;
        }
        if (ch == ']') {
            --arrayDepth;
            if (arrayDepth == 0) {
                break;
            }
            continue;
        }
        if (ch == '{') {
            if (objectDepth == 0) {
                objectStart = index;
            }
            ++objectDepth;
            continue;
        }
        if (ch == '}') {
            --objectDepth;
            if (objectDepth == 0 && objectStart != std::string::npos) {
                objects.push_back(manifestText.substr(objectStart, index - objectStart + 1));
                objectStart = std::string::npos;
            }
        }
    }

    return objects;
}

bool isSafeManifestRelativePath(const std::string& relativePath)
{
    if (relativePath.empty()) {
        return false;
    }
    if (relativePath.find('\\') != std::string::npos) {
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

std::optional<std::string> expectedChecksumRelevanceForPath(const std::string& relativePath)
{
    if (relativePath == "events/strategic_nexus_generated_events.txt") {
        return gameplayAffecting;
    }
    if (relativePath == "common/scripted_effects/strategic_nexus_generated_effects.txt") {
        return gameplayAffecting;
    }
    if (relativePath == "common/scripted_triggers/strategic_nexus_generated_triggers.txt") {
        return gameplayAffecting;
    }
    return std::nullopt;
}

} // namespace

std::string fnv1a64Hex(const std::string& text)
{
    std::uint64_t hash = 14695981039346656037ull;
    for (const unsigned char ch : text) {
        hash ^= ch;
        hash *= 1099511628211ull;
    }

    std::ostringstream output;
    output << std::hex << std::setw(16) << std::setfill('0') << hash;
    return output.str();
}

ManifestVerificationResult ManifestVerifier::verify(const std::filesystem::path& overlayDirectory) const
{
    ManifestVerificationResult result;
    if (overlayDirectory.empty()) {
        result.reason = "missing overlay directory";
        return result;
    }

    const auto manifestPath = overlayDirectory / manifestFileName;
    if (!std::filesystem::exists(manifestPath)) {
        result.reason = "missing generated overlay manifest";
        return result;
    }

    const std::string manifestText = common::readTextFile(manifestPath);
    if (!common::hasBalancedJsonDelimiters(manifestText)) {
        result.reason = "malformed generated overlay manifest";
        return result;
    }
    result.manifestHash = fnv1a64Hex(manifestText);

    const auto objects = extractManifestFileObjects(manifestText);
    if (objects.empty()) {
        result.reason = "manifest contains no generated files";
        return result;
    }

    bool allOk = true;
    std::unordered_set<std::string> expectedFiles;
    expectedFiles.insert(manifestFileName);
    for (const auto& objectText : objects) {
        ManifestFileVerification file;
        const auto path = extractStringFromObject(objectText, "path");
        const auto checksumRelevance = extractStringFromObject(objectText, "checksum_relevance");
        const auto expectedHash = extractStringFromObject(objectText, "hash");
        const auto expectedBytes = extractSizeFromObject(objectText, "byte_count");
        if (!path.has_value() || !checksumRelevance.has_value() || !expectedHash.has_value() || !expectedBytes.has_value()
            || !isSafeManifestRelativePath(*path)) {
            result.reason = "manifest contains invalid file entry";
            result.ok = false;
            return result;
        }

        const auto expectedChecksumRelevance = expectedChecksumRelevanceForPath(*path);
        if (!expectedChecksumRelevance.has_value() || *checksumRelevance != *expectedChecksumRelevance) {
            result.reason = "manifest contains invalid checksum relevance classification";
            result.ok = false;
            return result;
        }

        file.path = *path;
        file.checksumRelevance = *checksumRelevance;
        file.expectedHash = *expectedHash;
        file.expectedByteCount = *expectedBytes;
        if (expectedFiles.find(file.path) != expectedFiles.end()) {
            result.reason = "manifest contains duplicate file entry";
            result.ok = false;
            return result;
        }
        expectedFiles.insert(file.path);

        const auto filePath = overlayDirectory / std::filesystem::path(file.path);
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
    for (std::filesystem::recursive_directory_iterator it(overlayDirectory, iterError), end; it != end; it.increment(iterError)) {
        if (iterError) {
            break;
        }

        std::error_code typeError;
        if (!it->is_regular_file(typeError) || typeError) {
            continue;
        }

        std::error_code relativeError;
        const auto relative = std::filesystem::relative(it->path(), overlayDirectory, relativeError);
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
        result.reason = "accepted";
    } else if (unexpected) {
        result.reason = "generated overlay contains unexpected files";
    } else {
        result.reason = "generated overlay files do not match manifest";
    }
    return result;
}

} // namespace strategic_nexus::generated_overlay

