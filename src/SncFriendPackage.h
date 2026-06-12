// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus {

struct SncFriendPackageIdentity {
    std::string nodeId;
    std::string displayName;
    std::string signingPublicKey;
    std::string encryptionPublicKey;
    std::string fingerprint;
    std::vector<std::string> capabilities;
};

struct SncFriendRequestPackage {
    bool ok = false;
    std::string reason;
    SncFriendPackageIdentity sender;
    std::string createdAt;
    std::string expiresAt;
};

struct SncFriendAcceptancePackage {
    bool ok = false;
    std::string reason;
    std::string requestNodeId;
    std::string requestFingerprint;
    SncFriendPackageIdentity accepter;
    std::string createdAt;
};

struct SncTrustedFriend {
    SncFriendPackageIdentity identity;
    std::string localAlias;
    std::string trustState;
    bool autoSyncEnabled = false;
    std::string acceptedAt;
    std::string updatedAt;
};

struct SncFriendTrustStore {
    bool ok = false;
    std::string reason;
    int sourceSchemaVersion = 1;
    std::string schemaCompatibilityState = "current";
    std::string schemaCompatibilityNote;
    std::vector<SncTrustedFriend> friends;
};

struct SncFriendTrustStoreUpdateResult {
    bool ok = false;
    std::string reason;
    SncFriendTrustStore store;
};

struct SncFriendMpSyncEnvelopePackage {
    bool ok = false;
    std::string reason;
    SncFriendPackageIdentity sender;
    SncFriendPackageIdentity recipient;
    std::string campaignId;
    std::string overlayVersion;
    std::string packageManifestHash;
    std::string packageZipHash;
    std::string encryptedPayloadHash;
    std::string signingAlgorithm;
    std::string encryptionAlgorithm;
    std::string signature;
    std::string createdAt;
    std::size_t encryptedPayloadBytes = 0;
};

struct SncFriendMpSyncApplyGateResult {
    bool applyAllowed = false;
    std::string state;
    std::string reason;
};

struct SncFriendMpSyncInboxPlanResult {
    bool packageStagingAllowed = false;
    bool automaticDownloadEnabled = false;
    std::string state;
    std::string reason;
};

struct SncFriendMpSyncOutboxPlanResult {
    bool sendAllowed = false;
    bool transportEnabled = false;
    std::string state;
    std::string reason;
};

struct SncFriendMpSyncOutboxStageResult {
    bool ok = false;
    std::string state;
    std::string reason;
    std::filesystem::path stagedDirectory;
    std::filesystem::path stagedEnvelopePath;
    std::filesystem::path stagedEncryptedPayloadPath;
    std::filesystem::path stagedManifestPath;
};

struct SncFriendMpSyncInboxStageResult {
    bool ok = false;
    std::string state;
    std::string reason;
    std::filesystem::path stagedDirectory;
    std::filesystem::path stagedEnvelopePath;
    std::filesystem::path stagedEncryptedPayloadPath;
    std::filesystem::path stagedManifestPath;
};

SncFriendRequestPackage parseSncFriendRequestPackageJson(const std::string& json);
SncFriendAcceptancePackage parseSncFriendAcceptancePackageJson(const std::string& json);
SncFriendTrustStore parseSncFriendTrustStoreJson(const std::string& json);
SncFriendTrustStoreUpdateResult updateSncFriendTrustStoreEntry(
    const SncFriendTrustStore& store,
    const std::string& nodeId,
    const std::string& trustState,
    bool autoSyncEnabled,
    const std::string& updatedAt,
    const std::string& localAlias = "");
SncFriendMpSyncEnvelopePackage parseSncFriendMpSyncEnvelopePackageJson(const std::string& json);
SncFriendMpSyncApplyGateResult evaluateSncFriendMpSyncApplyGate(
    const SncFriendMpSyncEnvelopePackage& package,
    bool stellarisRunning);
SncFriendMpSyncInboxPlanResult planSncFriendMpSyncInboxStaging(
    const SncFriendMpSyncEnvelopePackage& package,
    bool encryptedPayloadPresent,
    bool stellarisRunning,
    bool friendAutoSyncEnabled);
SncFriendMpSyncOutboxPlanResult planSncFriendMpSyncOutboxSend(
    const SncFriendMpSyncEnvelopePackage& package,
    bool encryptedPayloadPresent,
    bool stellarisRunning,
    bool friendAutoSyncEnabled);
SncFriendMpSyncOutboxStageResult stageSncFriendMpSyncOutboxPackage(
    const SncFriendMpSyncEnvelopePackage& package,
    const std::filesystem::path& envelopeSourcePath,
    const std::filesystem::path& encryptedPayloadSourcePath,
    const std::filesystem::path& transportAdapterPath,
    bool stellarisRunning);
SncFriendMpSyncInboxStageResult stageSncFriendMpSyncInboxPackage(
    const SncFriendMpSyncEnvelopePackage& package,
    const std::filesystem::path& envelopeSourcePath,
    const std::filesystem::path& encryptedPayloadSourcePath,
    const std::filesystem::path& transportAdapterPath,
    bool stellarisRunning);
std::string serializeSncFriendRequestPackage(const SncFriendRequestPackage& package);
std::string serializeSncFriendAcceptancePackage(const SncFriendAcceptancePackage& package);
std::string serializeSncFriendTrustStore(const SncFriendTrustStore& store);
std::string serializeSncFriendMpSyncEnvelopePackage(const SncFriendMpSyncEnvelopePackage& package);
bool sncFriendAcceptanceMatchesRequest(
    const SncFriendRequestPackage& request,
    const SncFriendAcceptancePackage& acceptance);

} // namespace strategic_nexus
