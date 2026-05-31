// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "StellarisSavePathResolver.h"

#include <cstdlib>
#include <cctype>
#include <set>
#include <sstream>
#include <system_error>

namespace strategic_nexus {
namespace {

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

std::filesystem::path getenvPath(const char* name)
{
    const char* value = std::getenv(name);
    return value == nullptr ? std::filesystem::path() : std::filesystem::path(value);
}

std::filesystem::path stellarisSaveRootUnderDocuments(const std::filesystem::path& documentsRoot)
{
    return documentsRoot / "Paradox Interactive" / "Stellaris" / "save games";
}

std::filesystem::path stellarisSteamCloudSaveRoot(const std::filesystem::path& steamAccountRoot)
{
    return steamAccountRoot / "281990" / "remote" / "save games";
}

bool directoryExists(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::exists(path, error) && std::filesystem::is_directory(path, error);
}

void addCandidate(
    StellarisSaveRootDiscovery& discovery,
    std::set<std::string>& seen,
    const std::filesystem::path& path,
    const std::string& source)
{
    if (path.empty()) {
        return;
    }

    const auto key = path.lexically_normal().generic_string();
    if (!seen.insert(key).second) {
        return;
    }

    discovery.candidates.push_back({path, source, directoryExists(path)});
}

bool looksLikeSteamUserId(const std::filesystem::path& path)
{
    const auto name = path.filename().generic_string();
    if (name.empty()) {
        return false;
    }
    for (const char ch : name) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    return true;
}

std::vector<std::filesystem::path> steamAccountRootsFromUserDataRoot(const std::filesystem::path& userDataRoot)
{
    std::vector<std::filesystem::path> accountRoots;
    if (!directoryExists(userDataRoot)) {
        return accountRoots;
    }

    std::error_code error;
    for (std::filesystem::directory_iterator it(userDataRoot, error), end; it != end && !error; it.increment(error)) {
        if (error || !it->is_directory(error)) {
            continue;
        }
        if (looksLikeSteamUserId(it->path())) {
            accountRoots.push_back(it->path());
        }
    }
    return accountRoots;
}

} // namespace

StellarisSaveRootDiscovery StellarisSavePathResolver::discoverFromEnvironment() const
{
    std::vector<std::filesystem::path> oneDriveRoots;
    for (const char* name : {"OneDrive", "OneDriveConsumer", "OneDriveCommercial"}) {
        const auto path = getenvPath(name);
        if (!path.empty()) {
            oneDriveRoots.push_back(path);
        }
    }

    std::vector<std::filesystem::path> steamUserDataRoots;
    const auto addProgramFilesSteamRoot = [&](const char* envName) {
        const auto programFilesRoot = getenvPath(envName);
        if (!programFilesRoot.empty()) {
            steamUserDataRoots.push_back(programFilesRoot / "Steam" / "userdata");
        }
    };

    addProgramFilesSteamRoot("ProgramFiles(x86)");
    addProgramFilesSteamRoot("ProgramFiles");
    addProgramFilesSteamRoot("ProgramW6432");

    return buildCandidates(getenvPath("USERPROFILE"), oneDriveRoots, steamUserDataRoots);
}

StellarisSaveRootDiscovery StellarisSavePathResolver::buildCandidates(
    const std::filesystem::path& userProfile,
    const std::vector<std::filesystem::path>& oneDriveRoots,
    const std::vector<std::filesystem::path>& steamUserDataRoots) const
{
    StellarisSaveRootDiscovery discovery;
    std::set<std::string> seen;

    if (!userProfile.empty()) {
        addCandidate(
            discovery,
            seen,
            stellarisSaveRootUnderDocuments(userProfile / "Documents"),
            "user_profile_documents");
    }

    for (const auto& oneDriveRoot : oneDriveRoots) {
        addCandidate(
            discovery,
            seen,
            stellarisSaveRootUnderDocuments(oneDriveRoot / "Documents"),
            "onedrive_documents");
        addCandidate(
            discovery,
            seen,
            stellarisSaveRootUnderDocuments(oneDriveRoot / "Dokumenty"),
            "onedrive_dokumenty");
    }

    for (const auto& steamUserDataRoot : steamUserDataRoots) {
        for (const auto& accountRoot : steamAccountRootsFromUserDataRoot(steamUserDataRoot)) {
            addCandidate(
                discovery,
                seen,
                stellarisSteamCloudSaveRoot(accountRoot),
                "steam_cloud_userdata");
        }
    }

    return discovery;
}

std::string serializeStellarisSaveRootDiscovery(const StellarisSaveRootDiscovery& discovery)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"candidate_count\": " << discovery.candidates.size() << ",\n";
    json << "  \"candidates\": [\n";
    for (std::size_t i = 0; i < discovery.candidates.size(); ++i) {
        const auto& candidate = discovery.candidates[i];
        json << "    {\n";
        json << "      \"path\": \"" << jsonEscape(candidate.path.generic_string()) << "\",\n";
        json << "      \"source\": \"" << jsonEscape(candidate.source) << "\",\n";
        json << "      \"exists\": " << (candidate.exists ? "true" : "false") << "\n";
        json << "    }" << (i + 1 < discovery.candidates.size() ? "," : "") << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus

