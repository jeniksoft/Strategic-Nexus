// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SncFriendPackage.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void requireCondition(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        std::exit(1);
    }
}

strategic_nexus::SncFriendPackageIdentity makeIdentity(
    const std::string& nodeId,
    const std::string& displayName,
    const std::string& fingerprint)
{
    strategic_nexus::SncFriendPackageIdentity identity;
    identity.nodeId = nodeId;
    identity.displayName = displayName;
    identity.signingPublicKey = "ed25519:" + nodeId + "-signing-public-key";
    identity.encryptionPublicKey = "x25519:" + nodeId + "-encryption-public-key";
    identity.fingerprint = fingerprint;
    identity.capabilities = { "mp_package_sync", "handoff_sync" };
    return identity;
}

} // namespace

int main()
{
    strategic_nexus::SncFriendRequestPackage request;
    request.ok = true;
    request.sender = makeIdentity("snc-node-host-001", "Host SNC", "fp-host-001");
    request.createdAt = "2026-06-08T17:30:00Z";
    request.expiresAt = "2026-06-09T17:30:00Z";

    const auto requestJson = strategic_nexus::serializeSncFriendRequestPackage(request);
    const auto parsedRequest = strategic_nexus::parseSncFriendRequestPackageJson(requestJson);
    requireCondition(parsedRequest.ok, "valid friend request should parse");
    requireCondition(parsedRequest.sender.nodeId == "snc-node-host-001", "friend request should preserve node id");
    requireCondition(parsedRequest.sender.capabilities.size() == 2, "friend request should preserve capabilities");

    strategic_nexus::SncFriendAcceptancePackage acceptance;
    acceptance.ok = true;
    acceptance.requestNodeId = parsedRequest.sender.nodeId;
    acceptance.requestFingerprint = parsedRequest.sender.fingerprint;
    acceptance.accepter = makeIdentity("snc-node-client-001", "Client SNC", "fp-client-001");
    acceptance.createdAt = "2026-06-08T17:35:00Z";

    const auto acceptanceJson = strategic_nexus::serializeSncFriendAcceptancePackage(acceptance);
    const auto parsedAcceptance = strategic_nexus::parseSncFriendAcceptancePackageJson(acceptanceJson);
    requireCondition(parsedAcceptance.ok, "valid friend acceptance should parse");
    requireCondition(
        strategic_nexus::sncFriendAcceptanceMatchesRequest(parsedRequest, parsedAcceptance),
        "friend acceptance should match the original request");

    auto futureRequestJson = requestJson;
    const auto schemaMarker = futureRequestJson.find("\"schema_version\": 1");
    requireCondition(schemaMarker != std::string::npos, "request JSON should expose schema version");
    futureRequestJson.replace(schemaMarker, std::string("\"schema_version\": 1").size(), "\"schema_version\": 2");
    const auto futureRequest = strategic_nexus::parseSncFriendRequestPackageJson(futureRequestJson);
    requireCondition(!futureRequest.ok, "future friend request schema should fail closed");
    requireCondition(
        futureRequest.reason == "unsupported friend request package schema",
        "future friend request schema should explain compatibility failure");

    auto unsafeRequestJson = requestJson;
    const auto nodeMarker = unsafeRequestJson.find("snc-node-host-001");
    requireCondition(nodeMarker != std::string::npos, "request JSON should include node id");
    unsafeRequestJson.replace(nodeMarker, std::string("snc-node-host-001").size(), "bad node id with spaces");
    const auto unsafeRequest = strategic_nexus::parseSncFriendRequestPackageJson(unsafeRequestJson);
    requireCondition(!unsafeRequest.ok, "unsafe node id should fail closed");

    auto unsupportedCapabilityJson = requestJson;
    const auto capabilityMarker = unsupportedCapabilityJson.find("mp_package_sync");
    requireCondition(capabilityMarker != std::string::npos, "request JSON should include capability");
    unsupportedCapabilityJson.replace(capabilityMarker, std::string("mp_package_sync").size(), "raw_save_sync");
    const auto unsupportedCapability = strategic_nexus::parseSncFriendRequestPackageJson(unsupportedCapabilityJson);
    requireCondition(!unsupportedCapability.ok, "unsupported friend capability should fail closed");

    auto mismatchedAcceptance = parsedAcceptance;
    mismatchedAcceptance.requestFingerprint = "fp-other-request";
    requireCondition(
        !strategic_nexus::sncFriendAcceptanceMatchesRequest(parsedRequest, mismatchedAcceptance),
        "acceptance for another request should not match");

    auto selfAcceptance = parsedAcceptance;
    selfAcceptance.accepter.nodeId = parsedRequest.sender.nodeId;
    requireCondition(
        !strategic_nexus::sncFriendAcceptanceMatchesRequest(parsedRequest, selfAcceptance),
        "self-acceptance should not match");

    strategic_nexus::SncFriendTrustStore store;
    store.ok = true;
    strategic_nexus::SncTrustedFriend trustedFriend;
    trustedFriend.identity = parsedAcceptance.accepter;
    trustedFriend.localAlias = "Client";
    trustedFriend.trustState = "trusted";
    trustedFriend.autoSyncEnabled = true;
    trustedFriend.acceptedAt = "2026-06-08T17:36:00Z";
    trustedFriend.updatedAt = "2026-06-08T17:36:00Z";
    store.friends.push_back(trustedFriend);

    strategic_nexus::SncTrustedFriend revokedFriend;
    revokedFriend.identity = makeIdentity("snc-node-old-client-001", "Old Client SNC", "fp-old-client-001");
    revokedFriend.localAlias = "Old Client";
    revokedFriend.trustState = "revoked";
    revokedFriend.autoSyncEnabled = false;
    revokedFriend.acceptedAt = "2026-06-07T10:00:00Z";
    revokedFriend.updatedAt = "2026-06-08T10:00:00Z";
    store.friends.push_back(revokedFriend);

    const auto storeJson = strategic_nexus::serializeSncFriendTrustStore(store);
    const auto parsedStore = strategic_nexus::parseSncFriendTrustStoreJson(storeJson);
    requireCondition(parsedStore.ok, "valid friend trust store should parse");
    requireCondition(parsedStore.friends.size() == 2, "friend trust store should preserve friend count");
    requireCondition(parsedStore.friends[0].trustState == "trusted", "trusted friend state should roundtrip");
    requireCondition(parsedStore.friends[0].autoSyncEnabled, "trusted friend auto-sync flag should roundtrip");
    requireCondition(parsedStore.friends[1].trustState == "revoked", "revoked friend state should roundtrip");
    requireCondition(!parsedStore.friends[1].autoSyncEnabled, "revoked friend should keep auto-sync disabled");

    auto futureStoreJson = storeJson;
    const auto storeSchemaMarker = futureStoreJson.find("\"schema_version\": 1");
    requireCondition(storeSchemaMarker != std::string::npos, "trust store JSON should expose schema version");
    futureStoreJson.replace(storeSchemaMarker, std::string("\"schema_version\": 1").size(), "\"schema_version\": 2");
    const auto futureStore = strategic_nexus::parseSncFriendTrustStoreJson(futureStoreJson);
    requireCondition(!futureStore.ok, "future trust store schema should fail closed");

    auto duplicateStore = store;
    duplicateStore.friends[1].identity.nodeId = duplicateStore.friends[0].identity.nodeId;
    const auto duplicateStoreRead = strategic_nexus::parseSncFriendTrustStoreJson(
        strategic_nexus::serializeSncFriendTrustStore(duplicateStore));
    requireCondition(!duplicateStoreRead.ok, "duplicate friend node id should fail closed");

    auto missingAcceptedAtStore = store;
    missingAcceptedAtStore.friends[0].acceptedAt.clear();
    const auto missingAcceptedAtRead = strategic_nexus::parseSncFriendTrustStoreJson(
        strategic_nexus::serializeSncFriendTrustStore(missingAcceptedAtStore));
    requireCondition(!missingAcceptedAtRead.ok, "trusted friend without accepted_at should fail closed");

    auto blockedAutoSyncStore = store;
    blockedAutoSyncStore.friends[0].trustState = "blocked";
    blockedAutoSyncStore.friends[0].autoSyncEnabled = true;
    const auto blockedAutoSyncRead = strategic_nexus::parseSncFriendTrustStoreJson(
        strategic_nexus::serializeSncFriendTrustStore(blockedAutoSyncStore));
    requireCondition(!blockedAutoSyncRead.ok, "blocked friend with auto-sync enabled should fail closed");

    std::cout << "SNC friend package tests passed.\n";
    return 0;
}
