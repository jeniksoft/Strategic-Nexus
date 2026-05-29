// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "StellarisProcessDetector.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void requireCondition(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        std::exit(1);
    }
}

} // namespace

int main()
{
    const strategic_nexus::StellarisProcessDetector detector;

    const auto absent = detector.detectFromProcessNames({ "explorer.exe", "not_stellaris.exe", "dowser.exe" });
    requireCondition(absent.detectionAvailable, "process-name detector should be available");
    requireCondition(!absent.running, "detector should not match non-Stellaris process names");
    requireCondition(absent.reason == "Stellaris process not detected", "absent detector reason should be stable");

    const auto present = detector.detectFromProcessNames({ "explorer.exe", "Stellaris.exe" });
    requireCondition(present.running, "detector should match Stellaris.exe case-insensitively");
    requireCondition(present.matchedProcessNames.size() == 1, "detector should report matched process names");
    requireCondition(present.matchedProcessNames[0] == "Stellaris.exe", "detector should preserve matched process name");

    const auto pathPresent = detector.detectFromProcessNames({ "C:/Games/Stellaris/stellaris.exe" });
    requireCondition(pathPresent.running, "detector should match path-like process names");

    const auto quotedAndSpacedPresent = detector.detectFromProcessNames({ "  \"Stellaris.exe\"  " });
    requireCondition(quotedAndSpacedPresent.running, "detector should match quoted and spaced process names");

    const auto json = strategic_nexus::serializeStellarisProcessStatus(present);
    requireCondition(json.find("\"running\": true") != std::string::npos, "JSON should include running state");
    requireCondition(json.find("\"Stellaris.exe\"") != std::string::npos, "JSON should include matched process name");

    std::cout << "stellaris process detector tests passed.\n";
    return 0;
}
