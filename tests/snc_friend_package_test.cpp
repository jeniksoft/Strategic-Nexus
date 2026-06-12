// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SncFriendPackage.h"

#include <filesystem>
#include <fstream>
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
    requireCondition(parsedStore.sourceSchemaVersion == 1, "current trust store should preserve source schema version");
    requireCondition(
        parsedStore.schemaCompatibilityState == "current",
        "current trust store should expose current compatibility state");
    requireCondition(
        parsedStore.schemaCompatibilityNote.empty(),
        "current trust store should keep compatibility note empty");
    requireCondition(parsedStore.friends.size() == 2, "friend trust store should preserve friend count");
    requireCondition(parsedStore.friends[0].trustState == "trusted", "trusted friend state should roundtrip");
    requireCondition(parsedStore.friends[0].autoSyncEnabled, "trusted friend auto-sync flag should roundtrip");
    requireCondition(parsedStore.friends[1].trustState == "revoked", "revoked friend state should roundtrip");
    requireCondition(!parsedStore.friends[1].autoSyncEnabled, "revoked friend should keep auto-sync disabled");
    requireCondition(
        storeJson.find("\"schema_version\": 1") != std::string::npos &&
            storeJson.find("\"source_schema_version\": 1") != std::string::npos &&
            storeJson.find("\"schema_compatibility_state\": \"current\"") != std::string::npos,
        "friend trust store JSON should expose current compatibility metadata");

    auto legacyStoreJson = storeJson;
    const auto storeSchemaMarker = legacyStoreJson.find("\"schema_version\": 1");
    requireCondition(storeSchemaMarker != std::string::npos, "trust store JSON should expose schema version");
    legacyStoreJson.replace(storeSchemaMarker, std::string("\"schema_version\": 1").size(), "\"schema_version\": 0");
    const auto legacyStore = strategic_nexus::parseSncFriendTrustStoreJson(legacyStoreJson);
    requireCondition(legacyStore.ok, "legacy friend trust store should parse");
    requireCondition(legacyStore.sourceSchemaVersion == 0, "legacy trust store should preserve source schema version");
    requireCondition(
        legacyStore.schemaCompatibilityState == "partial_compatibility",
        "legacy trust store should expose partial compatibility");
    requireCondition(
        legacyStore.schemaCompatibilityNote == "migrated legacy friend trust store schema_version 0 to current schema_version 1",
        "legacy trust store should explain the migration note");
    requireCondition(
        strategic_nexus::serializeSncFriendTrustStore(legacyStore).find(
            "\"schema_compatibility_note\": \"migrated legacy friend trust store schema_version 0 to current schema_version 1\"") != std::string::npos,
        "legacy trust store JSON should preserve compatibility metadata");

    auto futureStoreJson = storeJson;
    const auto futureStoreSchemaMarker = futureStoreJson.find("\"schema_version\": 1");
    requireCondition(futureStoreSchemaMarker != std::string::npos, "trust store JSON should expose schema version");
    futureStoreJson.replace(
        futureStoreSchemaMarker,
        std::string("\"schema_version\": 1").size(),
        "\"schema_version\": 2");
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

    const auto revokedUpdate = strategic_nexus::updateSncFriendTrustStoreEntry(
        parsedStore,
        parsedStore.friends[0].identity.nodeId,
        "revoked",
        false,
        "2026-06-08T18:00:00Z");
    requireCondition(revokedUpdate.ok, "trusted friend should update to revoked");
    requireCondition(
        revokedUpdate.store.friends[0].trustState == "revoked" && !revokedUpdate.store.friends[0].autoSyncEnabled,
        "revoked trust-store update should disable auto-sync");

    const auto blockedUpdate = strategic_nexus::updateSncFriendTrustStoreEntry(
        parsedStore,
        parsedStore.friends[0].identity.nodeId,
        "blocked",
        false,
        "2026-06-08T18:01:00Z",
        "Client Blocked");
    requireCondition(blockedUpdate.ok, "trusted friend should update to blocked");
    requireCondition(
        blockedUpdate.store.friends[0].trustState == "blocked" &&
            blockedUpdate.store.friends[0].localAlias == "Client Blocked",
        "blocked trust-store update should preserve the requested alias");

    const auto disabledAutoSyncUpdate = strategic_nexus::updateSncFriendTrustStoreEntry(
        parsedStore,
        parsedStore.friends[0].identity.nodeId,
        "trusted",
        false,
        "2026-06-08T18:02:00Z");
    requireCondition(disabledAutoSyncUpdate.ok, "trusted friend should allow disabling auto-sync");
    requireCondition(
        disabledAutoSyncUpdate.store.friends[0].trustState == "trusted" &&
            !disabledAutoSyncUpdate.store.friends[0].autoSyncEnabled,
        "trusted update should allow disabling auto-sync");

    const auto blockedAutoSyncUpdate = strategic_nexus::updateSncFriendTrustStoreEntry(
        parsedStore,
        parsedStore.friends[0].identity.nodeId,
        "blocked",
        true,
        "2026-06-08T18:03:00Z");
    requireCondition(
        !blockedAutoSyncUpdate.ok,
        "blocked trust-state update should reject auto-sync");
    requireCondition(
        blockedAutoSyncUpdate.reason == "revoked or blocked friend cannot have auto-sync enabled",
        "blocked trust-state update should explain why auto-sync is rejected");

    strategic_nexus::SncFriendMpSyncEnvelopePackage syncEnvelope;
    syncEnvelope.ok = true;
    syncEnvelope.sender = parsedRequest.sender;
    syncEnvelope.recipient = parsedAcceptance.accepter;
    syncEnvelope.campaignId = "campaign-mp-001";
    syncEnvelope.overlayVersion = "overlay-v0-001";
    syncEnvelope.packageManifestHash = "sha256:manifest-001";
    syncEnvelope.packageZipHash = "sha256:zip-001";
    syncEnvelope.encryptedPayloadHash = "sha256:encrypted-payload-001";
    syncEnvelope.encryptedPayloadBytes = 4096;
    syncEnvelope.signingAlgorithm = "ed25519";
    syncEnvelope.encryptionAlgorithm = "x25519-xsalsa20-poly1305";
    syncEnvelope.signature = "ed25519:signature-001";
    syncEnvelope.createdAt = "2026-06-08T18:10:00Z";

    const auto syncEnvelopeJson = strategic_nexus::serializeSncFriendMpSyncEnvelopePackage(syncEnvelope);
    const auto parsedSyncEnvelope = strategic_nexus::parseSncFriendMpSyncEnvelopePackageJson(syncEnvelopeJson);
    requireCondition(parsedSyncEnvelope.ok, "valid friend mp sync envelope should parse");
    requireCondition(
        parsedSyncEnvelope.sender.nodeId == parsedRequest.sender.nodeId &&
            parsedSyncEnvelope.recipient.nodeId == parsedAcceptance.accepter.nodeId,
        "friend mp sync envelope should preserve sender and recipient identities");
    requireCondition(
        parsedSyncEnvelope.encryptedPayloadBytes == 4096,
        "friend mp sync envelope should preserve encrypted payload byte count");
    const auto syncApplyGate = strategic_nexus::evaluateSncFriendMpSyncApplyGate(
        parsedSyncEnvelope,
        false);
    requireCondition(
        !syncApplyGate.applyAllowed && syncApplyGate.state == "manual_metadata_only",
        "friend mp sync envelope should remain metadata-only when Stellaris is not running");
    const auto runningSyncApplyGate = strategic_nexus::evaluateSncFriendMpSyncApplyGate(
        parsedSyncEnvelope,
        true);
    requireCondition(
        !runningSyncApplyGate.applyAllowed && runningSyncApplyGate.state == "blocked_stellaris_running",
        "friend mp sync envelope apply gate should block while Stellaris is running");
    const auto inboxPlanWithPayload = strategic_nexus::planSncFriendMpSyncInboxStaging(
        parsedSyncEnvelope,
        true,
        false,
        true);
    requireCondition(
        !inboxPlanWithPayload.packageStagingAllowed &&
            !inboxPlanWithPayload.automaticDownloadEnabled &&
            inboxPlanWithPayload.state == "metadata_verified_transport_not_implemented",
        "friend mp sync inbox plan should not stage even when metadata and payload are present");
    const auto missingPayloadInboxPlan = strategic_nexus::planSncFriendMpSyncInboxStaging(
        parsedSyncEnvelope,
        false,
        false,
        true);
    requireCondition(
        !missingPayloadInboxPlan.packageStagingAllowed &&
            missingPayloadInboxPlan.state == "waiting_for_encrypted_payload",
        "friend mp sync inbox plan should wait for missing encrypted payload");
    const auto disabledAutoSyncInboxPlan = strategic_nexus::planSncFriendMpSyncInboxStaging(
        parsedSyncEnvelope,
        true,
        false,
        false);
    requireCondition(
        !disabledAutoSyncInboxPlan.packageStagingAllowed &&
            disabledAutoSyncInboxPlan.state == "waiting_for_owner_approval",
        "friend mp sync inbox plan should require owner approval when friend auto-sync is disabled");
    const auto runningInboxPlan = strategic_nexus::planSncFriendMpSyncInboxStaging(
        parsedSyncEnvelope,
        true,
        true,
        true);
    requireCondition(
        !runningInboxPlan.packageStagingAllowed && runningInboxPlan.state == "blocked_stellaris_running",
        "friend mp sync inbox plan should block while Stellaris is running");
    const auto outboxPlanWithPayload = strategic_nexus::planSncFriendMpSyncOutboxSend(
        parsedSyncEnvelope,
        true,
        false,
        true);
    requireCondition(
        !outboxPlanWithPayload.sendAllowed &&
            !outboxPlanWithPayload.transportEnabled &&
            outboxPlanWithPayload.state == "metadata_verified_transport_not_implemented",
        "friend mp sync outbox plan should not send even when metadata and payload are present");
    const auto missingPayloadOutboxPlan = strategic_nexus::planSncFriendMpSyncOutboxSend(
        parsedSyncEnvelope,
        false,
        false,
        true);
    requireCondition(
        !missingPayloadOutboxPlan.sendAllowed &&
            missingPayloadOutboxPlan.state == "waiting_for_encrypted_payload",
        "friend mp sync outbox plan should wait for missing encrypted payload");
    const auto disabledAutoSyncOutboxPlan = strategic_nexus::planSncFriendMpSyncOutboxSend(
        parsedSyncEnvelope,
        true,
        false,
        false);
    requireCondition(
        !disabledAutoSyncOutboxPlan.sendAllowed &&
            disabledAutoSyncOutboxPlan.state == "waiting_for_owner_approval",
        "friend mp sync outbox plan should require owner approval when friend auto-sync is disabled");
    const auto runningOutboxPlan = strategic_nexus::planSncFriendMpSyncOutboxSend(
        parsedSyncEnvelope,
        true,
        true,
        true);
    requireCondition(
        !runningOutboxPlan.sendAllowed && runningOutboxPlan.state == "blocked_stellaris_running",
        "friend mp sync outbox plan should block while Stellaris is running");

    const auto transportRoot = std::filesystem::temp_directory_path() / "snc_friend_mp_sync_transport_stage_test";
    std::filesystem::remove_all(transportRoot);
    std::filesystem::create_directories(transportRoot);
    const auto envelopeSourcePath = transportRoot / "friend_mp_sync_envelope.json";
    const auto payloadSourcePath = transportRoot / "friend_mp_sync_encrypted_payload.bin";
    {
        std::ofstream envelopeFile(envelopeSourcePath, std::ios::binary | std::ios::trunc);
        envelopeFile << syncEnvelopeJson;
    }
    {
        std::ofstream payloadFile(payloadSourcePath, std::ios::binary | std::ios::trunc);
        payloadFile << "encrypted-payload-bytes";
    }
    const auto transportAdapterPath = transportRoot / "selected_transport_adapter";
    std::filesystem::create_directories(transportAdapterPath);
    const auto stagedTransport = strategic_nexus::stageSncFriendMpSyncOutboxPackage(
        parsedSyncEnvelope,
        envelopeSourcePath,
        payloadSourcePath,
        transportAdapterPath,
        false);
    requireCondition(stagedTransport.ok, "friend mp sync outbox stage helper should stage a valid package");
    requireCondition(
        stagedTransport.state == "staged_for_transport",
        "friend mp sync outbox stage helper should expose staged state");
    requireCondition(
        std::filesystem::exists(stagedTransport.stagedEnvelopePath) &&
            std::filesystem::exists(stagedTransport.stagedEncryptedPayloadPath) &&
            std::filesystem::exists(stagedTransport.stagedManifestPath),
        "friend mp sync outbox stage helper should write staged files");
    requireCondition(
        stagedTransport.stagedDirectory.string().find("snc_friend_mp_sync_outbox") != std::string::npos,
        "friend mp sync outbox stage helper should stage into the outbox folder");

    const auto inboxStageTransportAdapterPath = transportRoot / "inbox_transport_adapter";
    std::filesystem::create_directories(inboxStageTransportAdapterPath);
    const auto stagedInboxTransport = strategic_nexus::stageSncFriendMpSyncInboxPackage(
        parsedSyncEnvelope,
        envelopeSourcePath,
        payloadSourcePath,
        inboxStageTransportAdapterPath,
        false);
    requireCondition(stagedInboxTransport.ok, "friend mp sync inbox stage helper should stage a valid package");
    requireCondition(
        stagedInboxTransport.state == "staged_for_inbox",
        "friend mp sync inbox stage helper should expose staged state");
    requireCondition(
        std::filesystem::exists(stagedInboxTransport.stagedEnvelopePath) &&
            std::filesystem::exists(stagedInboxTransport.stagedEncryptedPayloadPath) &&
            std::filesystem::exists(stagedInboxTransport.stagedManifestPath),
        "friend mp sync inbox stage helper should write staged files");
    requireCondition(
        stagedInboxTransport.stagedDirectory.string().find("snc_friend_mp_sync_inbox") != std::string::npos,
        "friend mp sync inbox stage helper should stage into the inbox folder");

    const auto blockedInboxTransport = strategic_nexus::stageSncFriendMpSyncInboxPackage(
        parsedSyncEnvelope,
        envelopeSourcePath,
        payloadSourcePath,
        inboxStageTransportAdapterPath,
        true);
    requireCondition(
        !blockedInboxTransport.ok && blockedInboxTransport.state == "blocked_stellaris_running",
        "friend mp sync inbox stage helper should block while Stellaris is running");

    const auto missingAdapterInboxTransport = strategic_nexus::stageSncFriendMpSyncInboxPackage(
        parsedSyncEnvelope,
        envelopeSourcePath,
        payloadSourcePath,
        transportRoot / "missing_inbox_adapter",
        false);
    requireCondition(
        !missingAdapterInboxTransport.ok && missingAdapterInboxTransport.state == "needs_attention",
        "friend mp sync inbox stage helper should fail closed for a missing adapter path");

    const auto blockedTransport = strategic_nexus::stageSncFriendMpSyncOutboxPackage(
        parsedSyncEnvelope,
        envelopeSourcePath,
        payloadSourcePath,
        transportAdapterPath,
        true);
    requireCondition(
        !blockedTransport.ok && blockedTransport.state == "blocked_stellaris_running",
        "friend mp sync outbox stage helper should block while Stellaris is running");

    const auto missingAdapterTransport = strategic_nexus::stageSncFriendMpSyncOutboxPackage(
        parsedSyncEnvelope,
        envelopeSourcePath,
        payloadSourcePath,
        transportRoot / "missing_adapter",
        false);
    requireCondition(
        !missingAdapterTransport.ok && missingAdapterTransport.state == "needs_attention",
        "friend mp sync outbox stage helper should fail closed for a missing adapter path");

    auto futureSyncEnvelopeJson = syncEnvelopeJson;
    const auto syncSchemaMarker = futureSyncEnvelopeJson.find("\"schema_version\": 1");
    requireCondition(syncSchemaMarker != std::string::npos, "sync envelope JSON should expose schema version");
    futureSyncEnvelopeJson.replace(
        syncSchemaMarker,
        std::string("\"schema_version\": 1").size(),
        "\"schema_version\": 2");
    const auto futureSyncEnvelope =
        strategic_nexus::parseSncFriendMpSyncEnvelopePackageJson(futureSyncEnvelopeJson);
    requireCondition(!futureSyncEnvelope.ok, "future sync envelope schema should fail closed");
    const auto invalidSyncApplyGate = strategic_nexus::evaluateSncFriendMpSyncApplyGate(
        futureSyncEnvelope,
        false);
    requireCondition(
        !invalidSyncApplyGate.applyAllowed && invalidSyncApplyGate.state == "invalid_envelope",
        "invalid friend mp sync envelope should not pass the apply gate");
    const auto invalidInboxPlan = strategic_nexus::planSncFriendMpSyncInboxStaging(
        futureSyncEnvelope,
        true,
        false,
        true);
    requireCondition(
        !invalidInboxPlan.packageStagingAllowed && invalidInboxPlan.state == "invalid_envelope",
        "invalid friend mp sync envelope should not pass the inbox plan gate");
    const auto invalidOutboxPlan = strategic_nexus::planSncFriendMpSyncOutboxSend(
        futureSyncEnvelope,
        true,
        false,
        true);
    requireCondition(
        !invalidOutboxPlan.sendAllowed && invalidOutboxPlan.state == "invalid_envelope",
        "invalid friend mp sync envelope should not pass the outbox plan gate");

    auto unsupportedEncryptionEnvelopeJson = syncEnvelopeJson;
    const auto encryptionMarker = unsupportedEncryptionEnvelopeJson.find("x25519-xsalsa20-poly1305");
    requireCondition(encryptionMarker != std::string::npos, "sync envelope JSON should include encryption algorithm");
    unsupportedEncryptionEnvelopeJson.replace(
        encryptionMarker,
        std::string("x25519-xsalsa20-poly1305").size(),
        "plaintext");
    const auto unsupportedEncryptionEnvelope =
        strategic_nexus::parseSncFriendMpSyncEnvelopePackageJson(unsupportedEncryptionEnvelopeJson);
    requireCondition(
        !unsupportedEncryptionEnvelope.ok,
        "unsupported sync envelope encryption algorithm should fail closed");

    auto selfRecipientEnvelope = syncEnvelope;
    selfRecipientEnvelope.recipient = syncEnvelope.sender;
    const auto selfRecipientRead = strategic_nexus::parseSncFriendMpSyncEnvelopePackageJson(
        strategic_nexus::serializeSncFriendMpSyncEnvelopePackage(selfRecipientEnvelope));
    requireCondition(!selfRecipientRead.ok, "sync envelope self-recipient should fail closed");

    auto missingCapabilityEnvelope = syncEnvelope;
    missingCapabilityEnvelope.recipient.capabilities = { "handoff_sync" };
    const auto missingCapabilityRead = strategic_nexus::parseSncFriendMpSyncEnvelopePackageJson(
        strategic_nexus::serializeSncFriendMpSyncEnvelopePackage(missingCapabilityEnvelope));
    requireCondition(
        !missingCapabilityRead.ok,
        "sync envelope without recipient mp_package_sync capability should fail closed");

    auto missingSignatureEnvelope = syncEnvelope;
    missingSignatureEnvelope.signature.clear();
    const auto missingSignatureRead = strategic_nexus::parseSncFriendMpSyncEnvelopePackageJson(
        strategic_nexus::serializeSncFriendMpSyncEnvelopePackage(missingSignatureEnvelope));
    requireCondition(!missingSignatureRead.ok, "sync envelope without signature should fail closed");

    std::cout << "SNC friend package tests passed.\n";
    return 0;
}
