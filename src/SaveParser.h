// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "CoreTypes.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus {

struct SaveParserFleetHeadline {
    std::string id;
    std::string name;
    std::string ownerCountryId;
    std::string militaryPower;
};

struct SaveParserSummary {
    bool ok = false;
    std::string reason;
    bool sourceWasArchive = false;
    bool hasMeta = false;
    bool hasGamestate = false;
    std::uintmax_t metaByteCount = 0;
    std::uintmax_t gamestateByteCount = 0;
    std::string saveVersion;
    std::string saveRevision;
    std::string saveName;
    std::string saveDate;
    std::string playerName;
    std::string playerCountryId;
    std::string empireName;
    std::string government;
    std::string authority;
    std::string founderSpeciesId;
    std::string founderSpeciesName;
    std::string capitalPlanetId;
    std::string capitalPlanetName;
    std::string homeSystemId;
    std::string homeSystemName;
    std::size_t ownedFleetCount = 0;
    std::size_t activeWarCount = 0;
    std::vector<SaveParserFleetHeadline> ownedFleets;
    std::vector<std::string> missingFields;
    std::vector<std::string> uncertainties;
};

class SaveParser {
public:
    GalaxyState parseSnapshot(const std::filesystem::path& savePath) const;
    SaveParserSummary parseSummary(const std::filesystem::path& savePath) const;
    std::string serializeSummaryJson(const SaveParserSummary& summary) const;
    std::string parseSummaryJson(const std::filesystem::path& savePath) const;
};

} // namespace strategic_nexus

