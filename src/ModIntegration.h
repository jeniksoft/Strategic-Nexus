// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "CoreTypes.h"
#include "StrategicRequest.h"

#include <filesystem>
#include <string>

namespace strategic_nexus {

class ModIntegration {
public:
    std::string renderDoctrineJson(const DoctrineDecision& decision) const;
    void writeDoctrineJson(const std::filesystem::path& outputPath, const DoctrineDecision& decision) const;
    std::string renderDoctrineJson(const StrategicRequest& request, const DoctrineDecision& decision) const;
    void writeDoctrineJson(const std::filesystem::path& outputPath, const StrategicRequest& request, const DoctrineDecision& decision) const;
    std::string renderErrorJson(const StrategicRequest& request, const std::string& message) const;
    void writeErrorJson(const std::filesystem::path& outputPath, const StrategicRequest& request, const std::string& message) const;
};

} // namespace strategic_nexus

