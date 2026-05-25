// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "AutosaveArchiveVerifier.h"

#include "common/FileUtil.h"
#include "common/JsonSanity.h"

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <algorithm>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>

namespace strategic_nexus {
namespace {

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool isPathWithinRoot(const std::filesystem::path& candidate, const std::filesystem::path& root)
{
    std::error_code candidateError;
    std::error_code rootError;
    const auto candidateCanonical = std::filesystem::weakly_canonical(candidate, candidateError);
    const auto rootCanonical = std::filesystem::weakly_canonical(root, rootError);
    if (candidateError || rootError) {
        return false;
    }

    auto candidateIt = candidateCanonical.begin();
    for (auto rootIt = rootCanonical.begin(); rootIt != rootCanonical.end(); ++rootIt) {
        if (candidateIt == candidateCanonical.end()) {
            return false;
        }

        const auto candidatePart = toLower(candidateIt->string());
        const auto rootPart = toLower(rootIt->string());
        if (candidatePart != rootPart) {
            return false;
        }
        ++candidateIt;
    }
    return true;
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

std::optional<std::uintmax_t> extractSizeFromObject(const std::string& objectText, const char* key)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (!std::regex_search(objectText, match, pattern)) {
        return std::nullopt;
    }

    try {
        return static_cast<std::uintmax_t>(std::stoull(match[1].str()));
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<std::string> extractEntryObjects(const std::string& manifestText)
{
    std::vector<std::string> objects;
    const std::regex entriesStartPattern("\"entries\"\\s*:\\s*\\[");
    std::smatch match;
    if (!std::regex_search(manifestText, match, entriesStartPattern)) {
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

std::string hashFileFnv1a64Hex(const std::filesystem::path& path)
{
    constexpr std::uint64_t offsetBasis = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = offsetBasis;

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }

    char buffer[4096];
    while (input) {
        input.read(buffer, sizeof(buffer));
        const auto count = input.gcount();
        for (std::streamsize index = 0; index < count; ++index) {
            hash ^= static_cast<unsigned char>(buffer[index]);
            hash *= prime;
        }
    }

    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

} // namespace

AutosaveArchiveVerificationResult AutosaveArchiveVerifier::verify(
    const std::filesystem::path& sessionArchiveDirectory) const
{
    AutosaveArchiveVerificationResult result;
    if (sessionArchiveDirectory.empty()) {
        result.reason = "missing archive directory";
        return result;
    }

    const auto manifestPath = sessionArchiveDirectory / "manifest.json";
    if (!std::filesystem::exists(manifestPath)) {
        result.reason = "missing autosave archive manifest";
        return result;
    }

    const auto manifestText = common::readTextFile(manifestPath);
    if (!common::hasBalancedJsonDelimiters(manifestText)) {
        result.reason = "malformed autosave archive manifest";
        return result;
    }

    const auto objects = extractEntryObjects(manifestText);
    if (objects.empty()) {
        result.reason = "autosave archive manifest contains no entries";
        return result;
    }

    bool allOk = true;
    std::size_t copiedEntries = 0;
    for (const auto& objectText : objects) {
        const auto status = extractStringFromObject(objectText, "status");
        if (!status.has_value() || *status != "copied") {
            continue;
        }
        ++copiedEntries;

        AutosaveArchiveFileVerification file;
        const auto archivedPath = extractStringFromObject(objectText, "archived_path");
        const auto expectedHash = extractStringFromObject(objectText, "content_hash");
        const auto expectedBytes = extractSizeFromObject(objectText, "byte_count");
        if (!archivedPath.has_value() || !expectedHash.has_value() || !expectedBytes.has_value()) {
            result.reason = "autosave archive manifest contains invalid copied entry";
            result.ok = false;
            return result;
        }

        file.path = *archivedPath;
        file.expectedHash = *expectedHash;
        file.expectedByteCount = *expectedBytes;

        const auto archivedPathValue = std::filesystem::path(file.path);
        const auto savesRoot = sessionArchiveDirectory / "saves";

        std::vector<std::filesystem::path> candidates;
        if (!archivedPathValue.empty()) {
            if (archivedPathValue.is_absolute()) {
                candidates.push_back(archivedPathValue);
            } else {
                if (archivedPathValue.has_parent_path()) {
                    candidates.push_back(archivedPathValue);
                    candidates.push_back(sessionArchiveDirectory / archivedPathValue);
                }
            }

            if (!archivedPathValue.filename().empty()) {
                candidates.push_back(savesRoot / archivedPathValue.filename());
            }
        }

        std::filesystem::path resolvedPath;
        for (const auto& candidate : candidates) {
            if (candidate.empty()) {
                continue;
            }
            if (!isPathWithinRoot(candidate, savesRoot)) {
                result.reason = "autosave archive manifest contains unsafe archived_path";
                result.ok = false;
                return result;
            }
            if (std::filesystem::exists(candidate)) {
                resolvedPath = candidate;
                break;
            }
        }

        file.exists = !resolvedPath.empty();
        if (file.exists) {
            std::error_code error;
            file.actualByteCount = std::filesystem::file_size(resolvedPath, error);
            if (error) {
                file.actualByteCount = 0;
            }
            file.actualHash = hashFileFnv1a64Hex(resolvedPath);
            file.hashMatches = file.actualHash == file.expectedHash;
            file.byteCountMatches = file.actualByteCount == file.expectedByteCount;
        }

        allOk = allOk && file.exists && file.hashMatches && file.byteCountMatches;
        result.files.push_back(file);
    }

    if (copiedEntries == 0) {
        result.reason = "autosave archive manifest contains no copied saves";
        return result;
    }

    result.ok = allOk;
    result.reason = allOk ? "accepted" : "autosave archive files do not match manifest";
    return result;
}

} // namespace strategic_nexus

