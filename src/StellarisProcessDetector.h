// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <string>
#include <vector>

namespace strategic_nexus {

struct StellarisProcessStatus {
    bool detectionAvailable = false;
    bool running = false;
    std::string reason;
    std::vector<std::string> matchedProcessNames;
};

class StellarisProcessDetector {
public:
    StellarisProcessStatus detectFromProcessNames(const std::vector<std::string>& processNames) const;
    StellarisProcessStatus detectFromSystem() const;
};

std::string serializeStellarisProcessStatus(const StellarisProcessStatus& status);

} // namespace strategic_nexus
