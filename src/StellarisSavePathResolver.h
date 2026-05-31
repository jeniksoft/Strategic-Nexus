// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus {

struct StellarisSaveRootCandidate {
    std::filesystem::path path;
    std::string source;
    bool exists = false;
};

struct StellarisSaveRootDiscovery {
    std::vector<StellarisSaveRootCandidate> candidates;
};

class StellarisSavePathResolver {
public:
    StellarisSaveRootDiscovery discoverFromEnvironment() const;
    StellarisSaveRootDiscovery buildCandidates(
        const std::filesystem::path& userProfile,
        const std::vector<std::filesystem::path>& oneDriveRoots,
        const std::vector<std::filesystem::path>& steamUserDataRoots = {}) const;
};

std::string serializeStellarisSaveRootDiscovery(const StellarisSaveRootDiscovery& discovery);

} // namespace strategic_nexus

