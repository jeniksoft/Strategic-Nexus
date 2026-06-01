// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "AutosaveArchiver.h"

#include "common/FileUtil.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>

namespace strategic_nexus {
namespace {

struct SaveSample {
    std::filesystem::path path;
    std::uintmax_t byteCount = 0;
    std::filesystem::file_time_type writeTime;
};

bool isSavFile(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return extension == ".sav";
}

std::string jsonEscape(const std::string& value)
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

std::string sanitizeFileStem(const std::string& value)
{
    std::string output;
    bool previousUnderscore = false;
    for (const char ch : value) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            output.push_back(ch);
            previousUnderscore = false;
        } else if (!previousUnderscore) {
            output.push_back('_');
            previousUnderscore = true;
        }
    }
    while (!output.empty() && output.back() == '_') {
        output.pop_back();
    }
    return output.empty() ? "autosave" : output;
}

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool isLiveSaveFile(const std::filesystem::path& path)
{
    if (!isSavFile(path)) {
        return false;
    }

    const auto filename = toLower(path.filename().string());
    return filename.rfind("autosave", 0) == 0 || filename == "ironman.sav";
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

std::map<std::string, SaveSample> sampleSaveFiles(const std::filesystem::path& saveRoot)
{
    std::map<std::string, SaveSample> samples;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(saveRoot, error)) {
        if (error) {
            break;
        }

        if (!entry.is_regular_file(error) || !isSavFile(entry.path())) {
            continue;
        }

        const auto byteCount = std::filesystem::file_size(entry.path(), error);
        if (error) {
            continue;
        }

        const auto writeTime = std::filesystem::last_write_time(entry.path(), error);
        if (error) {
            continue;
        }

        const auto key = entry.path().filename().generic_string();
        samples[key] = {entry.path(), byteCount, writeTime};
    }
    return samples;
}

std::map<std::string, SaveSample> sampleLiveSaveFiles(const std::filesystem::path& saveGamesRoot)
{
    std::map<std::string, SaveSample> samples;
    std::error_code error;
    const auto options = std::filesystem::directory_options::skip_permission_denied;
    for (auto iterator = std::filesystem::recursive_directory_iterator(saveGamesRoot, options, error);
         iterator != std::filesystem::recursive_directory_iterator();
         iterator.increment(error)) {
        if (error) {
            error.clear();
            continue;
        }

        std::error_code entryError;
        if (!iterator->is_regular_file(entryError) || entryError || !isLiveSaveFile(iterator->path())) {
            continue;
        }

        const auto byteCount = std::filesystem::file_size(iterator->path(), entryError);
        if (entryError) {
            continue;
        }

        const auto writeTime = std::filesystem::last_write_time(iterator->path(), entryError);
        if (entryError) {
            continue;
        }

        auto relativePath = std::filesystem::relative(iterator->path(), saveGamesRoot, entryError);
        if (entryError || relativePath.empty()) {
            relativePath = iterator->path().filename();
            entryError.clear();
        }

        samples[relativePath.generic_string()] = {iterator->path(), byteCount, writeTime};
    }
    return samples;
}

bool sameSample(const SaveSample& first, const SaveSample& second)
{
    return first.byteCount == second.byteCount && first.writeTime == second.writeTime;
}

std::filesystem::path archivePathFor(
    const std::filesystem::path& archiveDirectory,
    const std::string& key,
    const std::size_t index)
{
    std::ostringstream name;
    name << std::setw(3) << std::setfill('0') << index << "_" << sanitizeFileStem(std::filesystem::path(key).stem().string()) << ".sav";
    return archiveDirectory / name.str();
}

std::filesystem::path liveArchivePathFor(
    const std::filesystem::path& archiveDirectory,
    const std::string& key,
    const std::string& contentHash)
{
    auto sourceIdentity = std::filesystem::path(key).parent_path().generic_string();
    if (!sourceIdentity.empty()) {
        sourceIdentity += "_";
    }
    sourceIdentity += std::filesystem::path(key).stem().string();

    const auto hashPrefix = contentHash.substr(0, std::min<std::size_t>(contentHash.size(), 12));
    std::ostringstream name;
    name << "live_" << sanitizeFileStem(sourceIdentity) << "_" << hashPrefix << ".sav";
    return archiveDirectory / name.str();
}

AutosaveArchiveResult buildManifestResultFromArchivedFiles(
    const std::filesystem::path& sourceRoot,
    const std::filesystem::path& archiveDirectory,
    const std::string& sessionId,
    const bool sourceExists)
{
    AutosaveArchiveResult manifestResult;
    manifestResult.sourceExists = sourceExists;
    manifestResult.sourceRoot = sourceRoot;
    manifestResult.archiveDirectory = archiveDirectory;
    manifestResult.sessionId = sessionId;

    std::error_code error;
    if (!std::filesystem::exists(archiveDirectory, error) || error) {
        return manifestResult;
    }

    std::vector<std::filesystem::path> archivedFiles;
    for (const auto& entry : std::filesystem::directory_iterator(archiveDirectory, error)) {
        if (error) {
            break;
        }
        if (!entry.is_regular_file(error) || error || !isSavFile(entry.path())) {
            error.clear();
            continue;
        }
        archivedFiles.push_back(entry.path());
    }
    std::sort(archivedFiles.begin(), archivedFiles.end());

    for (const auto& path : archivedFiles) {
        AutosaveArchiveEntry archiveEntry;
        archiveEntry.archivedPath = (std::filesystem::path("saves") / path.filename()).generic_string();
        archiveEntry.status = "copied";
        archiveEntry.reason = "live_archive_copy";
        archiveEntry.hashAlgorithm = "fnv1a64";
        archiveEntry.contentHash = hashFileFnv1a64Hex(path);
        archiveEntry.byteCount = std::filesystem::file_size(path, error);
        if (error || archiveEntry.contentHash.empty()) {
            error.clear();
            continue;
        }
        ++manifestResult.copiedCount;
        manifestResult.entries.push_back(archiveEntry);
    }

    return manifestResult;
}

} // namespace

AutosaveArchiveResult AutosaveArchiver::archiveStableSaves(
    const std::filesystem::path& saveRoot,
    const std::filesystem::path& archiveRoot,
    const std::string& sessionId,
    const std::uint32_t stabilityDelayMs) const
{
    AutosaveArchiveResult result;
    result.sourceRoot = saveRoot;
    result.archiveDirectory = archiveRoot / sessionId / "saves";
    result.sessionId = sessionId;

    std::error_code error;
    result.sourceExists = std::filesystem::exists(saveRoot, error) && std::filesystem::is_directory(saveRoot, error);
    if (!result.sourceExists) {
        return result;
    }

    const auto firstSample = sampleSaveFiles(saveRoot);
    std::this_thread::sleep_for(std::chrono::milliseconds(stabilityDelayMs));
    const auto secondSample = sampleSaveFiles(saveRoot);

    std::vector<std::pair<std::string, SaveSample>> orderedSamples(firstSample.begin(), firstSample.end());
    std::sort(orderedSamples.begin(), orderedSamples.end(), [](const auto& left, const auto& right) {
        if (left.second.writeTime == right.second.writeTime) {
            return left.first < right.first;
        }
        return left.second.writeTime < right.second.writeTime;
    });

    std::size_t archiveIndex = 1;
    for (const auto& [key, first] : orderedSamples) {
        AutosaveArchiveEntry archiveEntry;
        archiveEntry.sourcePath = first.path.generic_string();
        archiveEntry.byteCount = first.byteCount;
        archiveEntry.hashAlgorithm = "fnv1a64";

        const auto second = secondSample.find(key);
        if (second == secondSample.end() || !sameSample(first, second->second)) {
            archiveEntry.status = "skipped";
            archiveEntry.reason = "not_stable";
            ++result.skippedCount;
            result.entries.push_back(archiveEntry);
            continue;
        }

        std::error_code directoryError;
        std::filesystem::create_directories(result.archiveDirectory, directoryError);
        if (directoryError) {
            archiveEntry.status = "skipped";
            archiveEntry.reason = "archive_directory_unavailable";
            ++result.skippedCount;
            result.entries.push_back(archiveEntry);
            continue;
        }

        const auto destination = archivePathFor(result.archiveDirectory, key, archiveIndex++);
        std::error_code copyError;
        std::filesystem::copy_file(first.path, destination, std::filesystem::copy_options::overwrite_existing, copyError);
        if (copyError) {
            archiveEntry.status = "skipped";
            archiveEntry.reason = "copy_failed";
            ++result.skippedCount;
            result.entries.push_back(archiveEntry);
            continue;
        }

        archiveEntry.archivedPath = destination.generic_string();
        archiveEntry.contentHash = hashFileFnv1a64Hex(destination);
        archiveEntry.status = archiveEntry.contentHash.empty() ? "skipped" : "copied";
        archiveEntry.reason = archiveEntry.contentHash.empty() ? "hash_failed" : "stable_copy";
        if (archiveEntry.status == "copied") {
            ++result.copiedCount;
        } else {
            ++result.skippedCount;
        }
        result.entries.push_back(archiveEntry);
    }

    common::writeTextFileAtomically(
        archiveRoot / sessionId / "manifest.json",
        serializeAutosaveArchiveManifest(result));

    return result;
}

AutosaveArchiveResult AutosaveArchiver::archiveLiveSaves(
    const std::filesystem::path& saveGamesRoot,
    const std::filesystem::path& archiveRoot,
    const std::string& sessionId,
    const std::uint32_t stabilityDelayMs) const
{
    AutosaveArchiveResult result;
    result.sourceRoot = saveGamesRoot;
    result.archiveDirectory = archiveRoot / sessionId / "saves";
    result.sessionId = sessionId;

    std::error_code error;
    result.sourceExists = std::filesystem::exists(saveGamesRoot, error) && std::filesystem::is_directory(saveGamesRoot, error);
    if (!result.sourceExists) {
        return result;
    }

    const auto firstSample = sampleLiveSaveFiles(saveGamesRoot);
    std::this_thread::sleep_for(std::chrono::milliseconds(stabilityDelayMs));
    const auto secondSample = sampleLiveSaveFiles(saveGamesRoot);

    std::vector<std::pair<std::string, SaveSample>> orderedSamples(firstSample.begin(), firstSample.end());
    std::sort(orderedSamples.begin(), orderedSamples.end(), [](const auto& left, const auto& right) {
        if (left.second.writeTime == right.second.writeTime) {
            return left.first < right.first;
        }
        return left.second.writeTime < right.second.writeTime;
    });

    for (const auto& [key, first] : orderedSamples) {
        AutosaveArchiveEntry archiveEntry;
        archiveEntry.sourcePath = first.path.generic_string();
        archiveEntry.byteCount = first.byteCount;
        archiveEntry.hashAlgorithm = "fnv1a64";

        const auto second = secondSample.find(key);
        if (second == secondSample.end() || !sameSample(first, second->second)) {
            archiveEntry.status = "skipped";
            archiveEntry.reason = "not_stable";
            ++result.skippedCount;
            result.entries.push_back(archiveEntry);
            continue;
        }

        archiveEntry.contentHash = hashFileFnv1a64Hex(first.path);
        if (archiveEntry.contentHash.empty()) {
            archiveEntry.status = "skipped";
            archiveEntry.reason = "hash_failed";
            ++result.skippedCount;
            result.entries.push_back(archiveEntry);
            continue;
        }

        std::error_code directoryError;
        std::filesystem::create_directories(result.archiveDirectory, directoryError);
        if (directoryError) {
            archiveEntry.status = "skipped";
            archiveEntry.reason = "archive_directory_unavailable";
            ++result.skippedCount;
            result.entries.push_back(archiveEntry);
            continue;
        }

        const auto destination = liveArchivePathFor(result.archiveDirectory, key, archiveEntry.contentHash);
        archiveEntry.archivedPath = destination.generic_string();
        if (std::filesystem::exists(destination, error) && !error) {
            archiveEntry.status = "skipped";
            archiveEntry.reason = "already_archived";
            ++result.skippedCount;
            result.entries.push_back(archiveEntry);
            continue;
        }
        error.clear();

        std::error_code copyError;
        std::filesystem::copy_file(first.path, destination, std::filesystem::copy_options::none, copyError);
        if (copyError) {
            archiveEntry.status = "skipped";
            archiveEntry.reason = "copy_failed";
            ++result.skippedCount;
            result.entries.push_back(archiveEntry);
            continue;
        }

        archiveEntry.status = "copied";
        archiveEntry.reason = "live_stable_copy";
        ++result.copiedCount;
        result.entries.push_back(archiveEntry);
    }

    writeLiveArchiveManifest(archiveRoot, sessionId, saveGamesRoot, result.sourceExists);

    return result;
}

AutosaveArchiveResult AutosaveArchiver::writeLiveArchiveManifest(
    const std::filesystem::path& archiveRoot,
    const std::string& sessionId,
    const std::filesystem::path& sourceRootLabel,
    const bool sourceExists) const
{
    const auto archiveDirectory = archiveRoot / sessionId / "saves";
    const auto manifestResult = buildManifestResultFromArchivedFiles(
        sourceRootLabel,
        archiveDirectory,
        sessionId,
        sourceExists);
    common::writeTextFileAtomically(
        archiveRoot / sessionId / "manifest.json",
        serializeAutosaveArchiveManifest(manifestResult));
    return manifestResult;
}

std::string serializeAutosaveArchiveManifest(const AutosaveArchiveResult& result)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"session_id\": \"" << jsonEscape(result.sessionId) << "\",\n";
    json << "  \"source_exists\": " << (result.sourceExists ? "true" : "false") << ",\n";
    json << "  \"source_root\": \"" << jsonEscape(result.sourceRoot.generic_string()) << "\",\n";
    json << "  \"archive_directory\": \"" << jsonEscape(result.archiveDirectory.generic_string()) << "\",\n";
    json << "  \"copied_count\": " << result.copiedCount << ",\n";
    json << "  \"skipped_count\": " << result.skippedCount << ",\n";
    json << "  \"entries\": [\n";
    for (std::size_t i = 0; i < result.entries.size(); ++i) {
        const auto& entry = result.entries[i];
        json << "    {\n";
        json << "      \"source_path\": \"" << jsonEscape(entry.sourcePath) << "\",\n";
        json << "      \"archived_path\": \"" << jsonEscape(entry.archivedPath) << "\",\n";
        json << "      \"status\": \"" << jsonEscape(entry.status) << "\",\n";
        json << "      \"reason\": \"" << jsonEscape(entry.reason) << "\",\n";
        json << "      \"byte_count\": " << entry.byteCount << ",\n";
        json << "      \"hash_algorithm\": \"" << jsonEscape(entry.hashAlgorithm) << "\",\n";
        json << "      \"content_hash\": \"" << jsonEscape(entry.contentHash) << "\"\n";
        json << "    }" << (i + 1 < result.entries.size() ? "," : "") << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus

