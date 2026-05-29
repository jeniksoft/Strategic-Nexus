// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <filesystem>
#include <chrono>
#include <string>

namespace strategic_nexus {

struct CompanionStatusConfig {
    std::filesystem::path archiveRoot;
    std::filesystem::path generatedOverlayDirectory;
    std::filesystem::path mpOverlayPackageDirectory;
    bool startWithWindowsEnabled = false;
};

struct CompanionStatusLoopConfig {
    CompanionStatusConfig statusConfig;
    std::filesystem::path statusOutputPath;
    std::chrono::milliseconds pollInterval = std::chrono::milliseconds(1000);
    int maxIterations = 1;
};

struct CompanionStatusLoopResult {
    bool ok = false;
    int iterationsRun = 0;
    std::string reason;
};

struct CompanionSubsystemStatus {
    std::string state;
    std::string reason;
    std::filesystem::path path;
    std::string manifestHash;
};

struct CompanionMpOverlayPackageStatus {
    std::string state;
    std::string reason;
    std::filesystem::path path;
    std::string campaignId;
    std::string overlayVersion;
    std::string gameVersion;
    std::string strategicNexusModVersion;
    std::string handoffStatus;
    std::string packageManifestHash;
    std::string statusText;
};

struct CompanionLifecycleStatus {
    bool startWithWindowsEnabled = false;
    std::string windowCloseBehavior = "minimize_to_tray";
    std::string explicitExitBehavior = "stop_without_restart";
    std::string crashRestartPolicy = "bounded_backoff_with_crash_loop_guard";
};

struct CompanionStatusSnapshot {
    std::string appName = "Strategic Nexus Companion";
    std::string abbreviation = "SNC";
    std::string generatedAtLocal;
    CompanionLifecycleStatus lifecycle;
    CompanionSubsystemStatus saveDiscovery;
    CompanionSubsystemStatus archive;
    CompanionSubsystemStatus generatedOverlay;
    CompanionMpOverlayPackageStatus mpOverlayPackage;
    CompanionSubsystemStatus statusCenter;
    std::string statusCenterSummaryText;
};

class StrategicNexusCompanion {
public:
    CompanionStatusSnapshot buildStatusSnapshot(const CompanionStatusConfig& config) const;
};

std::string serializeCompanionStatusSnapshot(const CompanionStatusSnapshot& snapshot);

CompanionStatusLoopResult runCompanionStatusLoop(const StrategicNexusCompanion& companion, const CompanionStatusLoopConfig& config);

} // namespace strategic_nexus
