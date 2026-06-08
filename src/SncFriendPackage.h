// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

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
    std::vector<SncTrustedFriend> friends;
};

SncFriendRequestPackage parseSncFriendRequestPackageJson(const std::string& json);
SncFriendAcceptancePackage parseSncFriendAcceptancePackageJson(const std::string& json);
SncFriendTrustStore parseSncFriendTrustStoreJson(const std::string& json);
std::string serializeSncFriendRequestPackage(const SncFriendRequestPackage& package);
std::string serializeSncFriendAcceptancePackage(const SncFriendAcceptancePackage& package);
std::string serializeSncFriendTrustStore(const SncFriendTrustStore& store);
bool sncFriendAcceptanceMatchesRequest(
    const SncFriendRequestPackage& request,
    const SncFriendAcceptancePackage& acceptance);

} // namespace strategic_nexus
