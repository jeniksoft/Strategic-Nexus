// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "CoreTypes.h"

#include <filesystem>
#include <string>

namespace strategic_nexus {

struct PersonalityProfileRecord {
    bool ok = false;
    std::string reason;
    std::string campaignId;
    std::string empireId;
    std::string sourceSaveDate;
    std::string sourceSavePath;
    std::string validatedUpdateSummary;
    double confidence = 0.0;
    bool zeroHistoryBootstrap = false;
    PersonalityProfile personality;
};

class PersonalityProfileStore {
public:
    std::filesystem::path profilePathFor(
        const std::filesystem::path& root,
        const std::string& campaignId,
        const std::string& empireId) const;

    bool save(const std::filesystem::path& root, const PersonalityProfileRecord& record) const;

    PersonalityProfileRecord load(
        const std::filesystem::path& root,
        const std::string& campaignId,
        const std::string& empireId) const;
};

std::string serializePersonalityProfileRecord(const PersonalityProfileRecord& record);

} // namespace strategic_nexus
