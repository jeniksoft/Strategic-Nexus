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

SncFriendRequestPackage parseSncFriendRequestPackageJson(const std::string& json);
SncFriendAcceptancePackage parseSncFriendAcceptancePackageJson(const std::string& json);
std::string serializeSncFriendRequestPackage(const SncFriendRequestPackage& package);
std::string serializeSncFriendAcceptancePackage(const SncFriendAcceptancePackage& package);
bool sncFriendAcceptanceMatchesRequest(
    const SncFriendRequestPackage& request,
    const SncFriendAcceptancePackage& acceptance);

} // namespace strategic_nexus
