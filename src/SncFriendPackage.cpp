// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SncFriendPackage.h"

#include "common/JsonExtract.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <regex>
#include <sstream>
#include <utility>

namespace strategic_nexus {
namespace {

constexpr std::size_t kMaxNodeIdLength = 80;
constexpr std::size_t kMaxDisplayNameLength = 80;
constexpr std::size_t kMaxPublicKeyLength = 180;
constexpr std::size_t kMaxFingerprintLength = 96;
constexpr std::size_t kMaxPackageIdLength = 96;
constexpr std::size_t kMaxHashLength = 128;
constexpr std::size_t kMaxSignatureLength = 256;

std::string jsonEscape(const std::string& value)
{
    std::ostringstream output;
    for (const char ch : value) {
        switch (ch) {
        case '\\': output << "\\\\"; break;
        case '"': output << "\\\""; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            output << (static_cast<unsigned char>(ch) < 0x20 ? ' ' : ch);
            break;
        }
    }
    return output.str();
}

std::optional<std::size_t> extractJsonSize(const std::string& json, const char* key)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*(\\d+)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return std::nullopt;
    }

    std::size_t parsed = 0;
    for (const char ch : match[1].str()) {
        if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
            return std::nullopt;
        }
        parsed = (parsed * 10) + static_cast<std::size_t>(ch - '0');
    }
    return parsed;
}

std::optional<bool> extractJsonBool(const std::string& json, const char* key)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return std::nullopt;
    }
    return match[1].str() == "true";
}

std::optional<std::string> extractArrayBody(const std::string& json, const char* key)
{
    const std::regex arrayStartPattern("\"" + std::string(key) + "\"\\s*:\\s*\\[");
    std::smatch match;
    if (!std::regex_search(json, match, arrayStartPattern)) {
        return std::nullopt;
    }

    const std::size_t bodyStart = static_cast<std::size_t>(match.position()) + match.length();
    int depth = 1;
    bool inString = false;
    bool escaped = false;

    for (std::size_t index = bodyStart; index < json.size(); ++index) {
        const char ch = json[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
        } else if (ch == '[') {
            ++depth;
        } else if (ch == ']') {
            --depth;
            if (depth == 0) {
                return json.substr(bodyStart, index - bodyStart);
            }
        }
    }

    return std::nullopt;
}

std::optional<std::string> extractObjectBody(const std::string& json, const char* key)
{
    const std::regex objectStartPattern("\"" + std::string(key) + "\"\\s*:\\s*\\{");
    std::smatch match;
    if (!std::regex_search(json, match, objectStartPattern)) {
        return std::nullopt;
    }

    const std::size_t objectStart = static_cast<std::size_t>(match.position()) + match.length() - 1;
    int depth = 0;
    bool inString = false;
    bool escaped = false;

    for (std::size_t index = objectStart; index < json.size(); ++index) {
        const char ch = json[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
        } else if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) {
                return json.substr(objectStart, index - objectStart + 1);
            }
        }
    }

    return std::nullopt;
}

std::vector<std::string> extractObjectBodies(const std::string& arrayBody)
{
    std::vector<std::string> objects;
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    std::size_t objectStart = std::string::npos;

    for (std::size_t index = 0; index < arrayBody.size(); ++index) {
        const char ch = arrayBody[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
            continue;
        }
        if (ch == '{') {
            if (depth == 0) {
                objectStart = index;
            }
            ++depth;
            continue;
        }
        if (ch == '}') {
            --depth;
            if (depth == 0 && objectStart != std::string::npos) {
                objects.push_back(arrayBody.substr(objectStart, index - objectStart + 1));
                objectStart = std::string::npos;
            }
        }
    }

    return objects;
}

std::string stringOrEmpty(const std::string& json, const char* key)
{
    return common::extractJsonString(json, key).value_or("");
}

bool isSafeToken(const std::string& value, const std::size_t maxLength)
{
    if (value.empty() || value.size() > maxLength) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](const char ch) {
        return std::isalnum(static_cast<unsigned char>(ch)) != 0
            || ch == '_' || ch == '-' || ch == '.' || ch == ':';
    });
}

bool isSafeDisplayName(const std::string& value)
{
    if (value.empty() || value.size() > kMaxDisplayNameLength) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](const char ch) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        return byte >= 0x20 && byte < 0x7f;
    });
}

bool isAllowedCapability(const std::string& capability)
{
    return capability == "mp_package_sync" || capability == "handoff_sync";
}

bool hasCapability(const SncFriendPackageIdentity& identity, const std::string& capability)
{
    return std::find(identity.capabilities.begin(), identity.capabilities.end(), capability)
        != identity.capabilities.end();
}

bool validateCapabilities(const std::vector<std::string>& capabilities)
{
    if (capabilities.empty() || capabilities.size() > 4) {
        return false;
    }

    std::vector<std::string> seen;
    for (const auto& capability : capabilities) {
        if (!isAllowedCapability(capability)) {
            return false;
        }
        if (std::find(seen.begin(), seen.end(), capability) != seen.end()) {
            return false;
        }
        seen.push_back(capability);
    }
    return true;
}

bool isSupportedTrustState(const std::string& state)
{
    return state == "trusted" || state == "revoked" || state == "blocked";
}

std::string validateIdentity(const SncFriendPackageIdentity& identity)
{
    if (!isSafeToken(identity.nodeId, kMaxNodeIdLength)) {
        return "friend package node_id is missing or unsafe";
    }
    if (!isSafeDisplayName(identity.displayName)) {
        return "friend package display_name is missing or unsafe";
    }
    if (!isSafeToken(identity.signingPublicKey, kMaxPublicKeyLength)) {
        return "friend package signing_public_key is missing or unsafe";
    }
    if (!isSafeToken(identity.encryptionPublicKey, kMaxPublicKeyLength)) {
        return "friend package encryption_public_key is missing or unsafe";
    }
    if (!isSafeToken(identity.fingerprint, kMaxFingerprintLength)) {
        return "friend package fingerprint is missing or unsafe";
    }
    if (!validateCapabilities(identity.capabilities)) {
        return "friend package capabilities are missing or unsupported";
    }
    return {};
}

std::string validateTrustedFriend(const SncTrustedFriend& friendEntry)
{
    const auto identityReason = validateIdentity(friendEntry.identity);
    if (!identityReason.empty()) {
        return identityReason;
    }
    if (!friendEntry.localAlias.empty() && !isSafeDisplayName(friendEntry.localAlias)) {
        return "trusted friend local_alias is unsafe";
    }
    if (!isSupportedTrustState(friendEntry.trustState)) {
        return "trusted friend state is unsupported";
    }
    if (friendEntry.trustState == "trusted" && friendEntry.acceptedAt.empty()) {
        return "trusted friend accepted_at is missing";
    }
    if (friendEntry.trustState != "trusted" && friendEntry.autoSyncEnabled) {
        return "revoked or blocked friend cannot have auto-sync enabled";
    }
    return {};
}

std::string validateMpSyncEnvelope(const SncFriendMpSyncEnvelopePackage& package)
{
    const auto senderReason = validateIdentity(package.sender);
    if (!senderReason.empty()) {
        return senderReason;
    }
    const auto recipientReason = validateIdentity(package.recipient);
    if (!recipientReason.empty()) {
        return recipientReason;
    }
    if (package.sender.nodeId == package.recipient.nodeId) {
        return "friend mp sync envelope sender and recipient must differ";
    }
    if (!hasCapability(package.sender, "mp_package_sync") ||
        !hasCapability(package.recipient, "mp_package_sync")) {
        return "friend mp sync envelope requires mp_package_sync capability";
    }
    if (!isSafeToken(package.campaignId, kMaxPackageIdLength) ||
        !isSafeToken(package.overlayVersion, kMaxPackageIdLength)) {
        return "friend mp sync envelope campaign or overlay identity is missing or unsafe";
    }
    if (!isSafeToken(package.packageManifestHash, kMaxHashLength) ||
        !isSafeToken(package.packageZipHash, kMaxHashLength) ||
        !isSafeToken(package.encryptedPayloadHash, kMaxHashLength)) {
        return "friend mp sync envelope package hashes are missing or unsafe";
    }
    if (package.signingAlgorithm != "ed25519") {
        return "friend mp sync envelope signing algorithm is unsupported";
    }
    if (package.encryptionAlgorithm != "x25519-xsalsa20-poly1305") {
        return "friend mp sync envelope encryption algorithm is unsupported";
    }
    if (!isSafeToken(package.signature, kMaxSignatureLength)) {
        return "friend mp sync envelope signature is missing or unsafe";
    }
    if (package.createdAt.empty()) {
        return "friend mp sync envelope created_at is missing";
    }
    if (package.encryptedPayloadBytes == 0) {
        return "friend mp sync envelope encrypted payload byte count is missing";
    }
    return {};
}

SncFriendPackageIdentity parseIdentity(const std::string& json)
{
    SncFriendPackageIdentity identity;
    identity.nodeId = stringOrEmpty(json, "node_id");
    identity.displayName = stringOrEmpty(json, "display_name");
    identity.signingPublicKey = stringOrEmpty(json, "signing_public_key");
    identity.encryptionPublicKey = stringOrEmpty(json, "encryption_public_key");
    identity.fingerprint = stringOrEmpty(json, "fingerprint");
    identity.capabilities = common::extractJsonStringArray(json, "capabilities");
    return identity;
}

SncTrustedFriend parseTrustedFriend(const std::string& json)
{
    SncTrustedFriend friendEntry;
    friendEntry.identity = parseIdentity(json);
    friendEntry.localAlias = stringOrEmpty(json, "local_alias");
    friendEntry.trustState = stringOrEmpty(json, "trust_state");
    friendEntry.autoSyncEnabled = extractJsonBool(json, "auto_sync_enabled").value_or(false);
    friendEntry.acceptedAt = stringOrEmpty(json, "accepted_at");
    friendEntry.updatedAt = stringOrEmpty(json, "updated_at");
    return friendEntry;
}

void writeStringArray(std::ostringstream& output, const std::vector<std::string>& values)
{
    output << "[";
    for (std::size_t index = 0; index < values.size(); ++index) {
        output << (index == 0 ? "" : ", ") << "\"" << jsonEscape(values[index]) << "\"";
    }
    output << "]";
}

void writeIdentity(std::ostringstream& output, const SncFriendPackageIdentity& identity)
{
    output << "  \"node_id\": \"" << jsonEscape(identity.nodeId) << "\",\n";
    output << "  \"display_name\": \"" << jsonEscape(identity.displayName) << "\",\n";
    output << "  \"signing_public_key\": \"" << jsonEscape(identity.signingPublicKey) << "\",\n";
    output << "  \"encryption_public_key\": \"" << jsonEscape(identity.encryptionPublicKey) << "\",\n";
    output << "  \"fingerprint\": \"" << jsonEscape(identity.fingerprint) << "\",\n";
    output << "  \"capabilities\": ";
    writeStringArray(output, identity.capabilities);
    output << "\n";
}

void writeTrustedFriend(std::ostringstream& output, const SncTrustedFriend& friendEntry)
{
    output << "    {\n";
    output << "      \"node_id\": \"" << jsonEscape(friendEntry.identity.nodeId) << "\",\n";
    output << "      \"display_name\": \"" << jsonEscape(friendEntry.identity.displayName) << "\",\n";
    output << "      \"local_alias\": \"" << jsonEscape(friendEntry.localAlias) << "\",\n";
    output << "      \"signing_public_key\": \"" << jsonEscape(friendEntry.identity.signingPublicKey) << "\",\n";
    output << "      \"encryption_public_key\": \"" << jsonEscape(friendEntry.identity.encryptionPublicKey) << "\",\n";
    output << "      \"fingerprint\": \"" << jsonEscape(friendEntry.identity.fingerprint) << "\",\n";
    output << "      \"capabilities\": ";
    writeStringArray(output, friendEntry.identity.capabilities);
    output << ",\n";
    output << "      \"trust_state\": \"" << jsonEscape(friendEntry.trustState) << "\",\n";
    output << "      \"auto_sync_enabled\": " << (friendEntry.autoSyncEnabled ? "true" : "false") << ",\n";
    output << "      \"accepted_at\": \"" << jsonEscape(friendEntry.acceptedAt) << "\",\n";
    output << "      \"updated_at\": \"" << jsonEscape(friendEntry.updatedAt) << "\"\n";
    output << "    }";
}

void writeIndentedIdentity(
    std::ostringstream& output,
    const SncFriendPackageIdentity& identity,
    const char* indent)
{
    output << indent << "\"node_id\": \"" << jsonEscape(identity.nodeId) << "\",\n";
    output << indent << "\"display_name\": \"" << jsonEscape(identity.displayName) << "\",\n";
    output << indent << "\"signing_public_key\": \"" << jsonEscape(identity.signingPublicKey) << "\",\n";
    output << indent << "\"encryption_public_key\": \"" << jsonEscape(identity.encryptionPublicKey) << "\",\n";
    output << indent << "\"fingerprint\": \"" << jsonEscape(identity.fingerprint) << "\",\n";
    output << indent << "\"capabilities\": ";
    writeStringArray(output, identity.capabilities);
    output << "\n";
}

} // namespace

SncFriendRequestPackage parseSncFriendRequestPackageJson(const std::string& json)
{
    SncFriendRequestPackage package;
    if (json.empty()) {
        package.reason = "friend request package json is empty";
        return package;
    }

    const auto schemaVersion = extractJsonSize(json, "schema_version");
    if (!schemaVersion.has_value() || *schemaVersion != 1) {
        package.reason = "unsupported friend request package schema";
        return package;
    }
    if (stringOrEmpty(json, "package_type") != "snc_friend_request") {
        package.reason = "friend request package type is unsupported";
        return package;
    }

    package.sender = parseIdentity(json);
    package.createdAt = stringOrEmpty(json, "created_at");
    package.expiresAt = stringOrEmpty(json, "expires_at");

    package.reason = validateIdentity(package.sender);
    if (!package.reason.empty()) {
        return package;
    }
    if (package.createdAt.empty()) {
        package.reason = "friend request package created_at is missing";
        return package;
    }

    package.ok = true;
    package.reason = "friend request package parsed";
    return package;
}

SncFriendAcceptancePackage parseSncFriendAcceptancePackageJson(const std::string& json)
{
    SncFriendAcceptancePackage package;
    if (json.empty()) {
        package.reason = "friend acceptance package json is empty";
        return package;
    }

    const auto schemaVersion = extractJsonSize(json, "schema_version");
    if (!schemaVersion.has_value() || *schemaVersion != 1) {
        package.reason = "unsupported friend acceptance package schema";
        return package;
    }
    if (stringOrEmpty(json, "package_type") != "snc_friend_acceptance") {
        package.reason = "friend acceptance package type is unsupported";
        return package;
    }

    package.requestNodeId = stringOrEmpty(json, "request_node_id");
    package.requestFingerprint = stringOrEmpty(json, "request_fingerprint");
    package.accepter = parseIdentity(json);
    package.createdAt = stringOrEmpty(json, "created_at");

    if (!isSafeToken(package.requestNodeId, kMaxNodeIdLength)
        || !isSafeToken(package.requestFingerprint, kMaxFingerprintLength)) {
        package.reason = "friend acceptance request identity is missing or unsafe";
        return package;
    }

    package.reason = validateIdentity(package.accepter);
    if (!package.reason.empty()) {
        return package;
    }
    if (package.createdAt.empty()) {
        package.reason = "friend acceptance package created_at is missing";
        return package;
    }

    package.ok = true;
    package.reason = "friend acceptance package parsed";
    return package;
}

SncFriendTrustStore parseSncFriendTrustStoreJson(const std::string& json)
{
    SncFriendTrustStore store;
    if (json.empty()) {
        store.reason = "friend trust store json is empty";
        return store;
    }

    const auto schemaVersion = extractJsonSize(json, "schema_version");
    if (!schemaVersion.has_value() || *schemaVersion != 1) {
        store.reason = "unsupported friend trust store schema";
        return store;
    }

    const auto friendsBody = extractArrayBody(json, "friends");
    if (!friendsBody.has_value()) {
        store.reason = "friend trust store friends array is missing";
        return store;
    }

    std::vector<std::string> nodeIds;
    for (const auto& object : extractObjectBodies(*friendsBody)) {
        auto friendEntry = parseTrustedFriend(object);
        store.reason = validateTrustedFriend(friendEntry);
        if (!store.reason.empty()) {
            store.friends.clear();
            return store;
        }
        if (std::find(nodeIds.begin(), nodeIds.end(), friendEntry.identity.nodeId) != nodeIds.end()) {
            store.reason = "friend trust store contains duplicate node_id";
            store.friends.clear();
            return store;
        }
        nodeIds.push_back(friendEntry.identity.nodeId);
        store.friends.push_back(std::move(friendEntry));
    }

    store.ok = true;
    store.reason = "friend trust store parsed";
    return store;
}

SncFriendTrustStoreUpdateResult updateSncFriendTrustStoreEntry(
    const SncFriendTrustStore& store,
    const std::string& nodeId,
    const std::string& trustState,
    const bool autoSyncEnabled,
    const std::string& updatedAt,
    const std::string& localAlias)
{
    SncFriendTrustStoreUpdateResult result;
    if (!store.ok) {
        result.reason = store.reason.empty()
            ? "friend trust store is invalid"
            : store.reason;
        return result;
    }
    if (!isSafeToken(nodeId, kMaxNodeIdLength)) {
        result.reason = "friend trust store node_id is missing or unsafe";
        return result;
    }
    if (!isSupportedTrustState(trustState)) {
        result.reason = "friend trust state is unsupported";
        return result;
    }
    if (!isSafeToken(updatedAt, kMaxPackageIdLength)) {
        result.reason = "friend trust store updated_at is missing or unsafe";
        return result;
    }
    if (trustState != "trusted" && autoSyncEnabled) {
        result.reason = "revoked or blocked friend cannot have auto-sync enabled";
        return result;
    }
    if (!localAlias.empty() && !isSafeDisplayName(localAlias)) {
        result.reason = "trusted friend local_alias is unsafe";
        return result;
    }

    result.store = store;
    const auto friendIt = std::find_if(
        result.store.friends.begin(),
        result.store.friends.end(),
        [&nodeId](const SncTrustedFriend& friendEntry) {
            return friendEntry.identity.nodeId == nodeId;
        });
    if (friendIt == result.store.friends.end()) {
        result.reason = "friend trust store does not contain requested node_id";
        result.store.ok = false;
        return result;
    }

    friendIt->trustState = trustState;
    friendIt->autoSyncEnabled = autoSyncEnabled;
    friendIt->updatedAt = updatedAt;
    if (!localAlias.empty()) {
        friendIt->localAlias = localAlias;
    }

    const auto updatedJson = serializeSncFriendTrustStore(result.store);
    const auto parsedUpdatedStore = parseSncFriendTrustStoreJson(updatedJson);
    if (!parsedUpdatedStore.ok) {
        result.reason = parsedUpdatedStore.reason.empty()
            ? "friend trust store update failed validation"
            : parsedUpdatedStore.reason;
        result.store.ok = false;
        return result;
    }

    result.ok = true;
    result.reason = "friend trust store entry updated";
    result.store = std::move(parsedUpdatedStore);
    return result;
}

SncFriendMpSyncEnvelopePackage parseSncFriendMpSyncEnvelopePackageJson(const std::string& json)
{
    SncFriendMpSyncEnvelopePackage package;
    if (json.empty()) {
        package.reason = "friend mp sync envelope json is empty";
        return package;
    }

    const auto schemaVersion = extractJsonSize(json, "schema_version");
    if (!schemaVersion.has_value() || *schemaVersion != 1) {
        package.reason = "unsupported friend mp sync envelope schema";
        return package;
    }
    if (stringOrEmpty(json, "package_type") != "snc_friend_mp_package_sync") {
        package.reason = "friend mp sync envelope package type is unsupported";
        return package;
    }

    const auto senderBody = extractObjectBody(json, "sender");
    const auto recipientBody = extractObjectBody(json, "recipient");
    package.sender = parseIdentity(senderBody.value_or(""));
    package.recipient = parseIdentity(recipientBody.value_or(""));
    package.campaignId = stringOrEmpty(json, "campaign_id");
    package.overlayVersion = stringOrEmpty(json, "overlay_version");
    package.packageManifestHash = stringOrEmpty(json, "package_manifest_hash");
    package.packageZipHash = stringOrEmpty(json, "package_zip_hash");
    package.encryptedPayloadHash = stringOrEmpty(json, "encrypted_payload_hash");
    package.signingAlgorithm = stringOrEmpty(json, "signing_algorithm");
    package.encryptionAlgorithm = stringOrEmpty(json, "encryption_algorithm");
    package.signature = stringOrEmpty(json, "signature");
    package.createdAt = stringOrEmpty(json, "created_at");
    package.encryptedPayloadBytes = extractJsonSize(json, "encrypted_payload_bytes").value_or(0);

    package.reason = validateMpSyncEnvelope(package);
    if (!package.reason.empty()) {
        return package;
    }

    package.ok = true;
    package.reason = "friend mp sync envelope parsed";
    return package;
}

SncFriendMpSyncApplyGateResult evaluateSncFriendMpSyncApplyGate(
    const SncFriendMpSyncEnvelopePackage& package,
    const bool stellarisRunning)
{
    SncFriendMpSyncApplyGateResult result;
    if (!package.ok) {
        result.state = "invalid_envelope";
        result.reason = package.reason.empty()
            ? "friend mp sync envelope is invalid"
            : package.reason;
        return result;
    }
    if (stellarisRunning) {
        result.state = "blocked_stellaris_running";
        result.reason = "Stellaris is running; friend MP package apply is deferred";
        return result;
    }

    result.state = "manual_metadata_only";
    result.reason = "friend MP sync envelope verified; automatic download, staging, and package apply remain disabled";
    return result;
}

SncFriendMpSyncInboxPlanResult planSncFriendMpSyncInboxStaging(
    const SncFriendMpSyncEnvelopePackage& package,
    const bool encryptedPayloadPresent,
    const bool stellarisRunning,
    const bool friendAutoSyncEnabled)
{
    SncFriendMpSyncInboxPlanResult result;
    if (!package.ok) {
        result.state = "invalid_envelope";
        result.reason = package.reason.empty()
            ? "friend mp sync envelope is invalid"
            : package.reason;
        return result;
    }
    if (stellarisRunning) {
        result.state = "blocked_stellaris_running";
        result.reason = "Stellaris is running; friend MP package inbox staging is deferred";
        return result;
    }
    if (!encryptedPayloadPresent) {
        result.state = "waiting_for_encrypted_payload";
        result.reason = "friend MP sync envelope is valid but the encrypted payload is not present";
        return result;
    }
    if (!friendAutoSyncEnabled) {
        result.state = "waiting_for_owner_approval";
        result.reason = "friend auto-sync is disabled; package remains manual review only";
        return result;
    }

    result.state = "metadata_verified_transport_not_implemented";
    result.reason =
        "friend MP sync metadata and payload presence verified; signed/encrypted transport, decryption, and staging remain disabled";
    return result;
}

SncFriendMpSyncOutboxPlanResult planSncFriendMpSyncOutboxSend(
    const SncFriendMpSyncEnvelopePackage& package,
    const bool encryptedPayloadPresent,
    const bool stellarisRunning,
    const bool friendAutoSyncEnabled)
{
    SncFriendMpSyncOutboxPlanResult result;
    if (!package.ok) {
        result.state = "invalid_envelope";
        result.reason = package.reason.empty()
            ? "friend mp sync envelope is invalid"
            : package.reason;
        return result;
    }
    if (stellarisRunning) {
        result.state = "blocked_stellaris_running";
        result.reason = "Stellaris is running; friend MP package outbox send is deferred";
        return result;
    }
    if (!encryptedPayloadPresent) {
        result.state = "waiting_for_encrypted_payload";
        result.reason = "friend MP sync envelope is valid but the encrypted payload is not present";
        return result;
    }
    if (!friendAutoSyncEnabled) {
        result.state = "waiting_for_owner_approval";
        result.reason = "friend auto-sync is disabled; package remains manual share only";
        return result;
    }

    result.state = "metadata_verified_transport_not_implemented";
    result.reason =
        "friend MP sync metadata and payload presence verified; signed/encrypted transport remains disabled";
    return result;
}

std::string serializeSncFriendRequestPackage(const SncFriendRequestPackage& package)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"package_type\": \"snc_friend_request\",\n";
    writeIdentity(json, package.sender);
    json << ",\n";
    json << "  \"created_at\": \"" << jsonEscape(package.createdAt) << "\",\n";
    json << "  \"expires_at\": \"" << jsonEscape(package.expiresAt) << "\"\n";
    json << "}\n";
    return json.str();
}

std::string serializeSncFriendAcceptancePackage(const SncFriendAcceptancePackage& package)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"package_type\": \"snc_friend_acceptance\",\n";
    json << "  \"request_node_id\": \"" << jsonEscape(package.requestNodeId) << "\",\n";
    json << "  \"request_fingerprint\": \"" << jsonEscape(package.requestFingerprint) << "\",\n";
    writeIdentity(json, package.accepter);
    json << ",\n";
    json << "  \"created_at\": \"" << jsonEscape(package.createdAt) << "\"\n";
    json << "}\n";
    return json.str();
}

std::string serializeSncFriendTrustStore(const SncFriendTrustStore& store)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"friends\": [\n";
    for (std::size_t index = 0; index < store.friends.size(); ++index) {
        writeTrustedFriend(json, store.friends[index]);
        json << (index + 1 < store.friends.size() ? "," : "") << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

std::string serializeSncFriendMpSyncEnvelopePackage(const SncFriendMpSyncEnvelopePackage& package)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"package_type\": \"snc_friend_mp_package_sync\",\n";
    json << "  \"sender\": {\n";
    writeIndentedIdentity(json, package.sender, "    ");
    json << "  },\n";
    json << "  \"recipient\": {\n";
    writeIndentedIdentity(json, package.recipient, "    ");
    json << "  },\n";
    json << "  \"campaign_id\": \"" << jsonEscape(package.campaignId) << "\",\n";
    json << "  \"overlay_version\": \"" << jsonEscape(package.overlayVersion) << "\",\n";
    json << "  \"package_manifest_hash\": \"" << jsonEscape(package.packageManifestHash) << "\",\n";
    json << "  \"package_zip_hash\": \"" << jsonEscape(package.packageZipHash) << "\",\n";
    json << "  \"encrypted_payload_hash\": \"" << jsonEscape(package.encryptedPayloadHash) << "\",\n";
    json << "  \"encrypted_payload_bytes\": " << package.encryptedPayloadBytes << ",\n";
    json << "  \"signing_algorithm\": \"" << jsonEscape(package.signingAlgorithm) << "\",\n";
    json << "  \"encryption_algorithm\": \"" << jsonEscape(package.encryptionAlgorithm) << "\",\n";
    json << "  \"signature\": \"" << jsonEscape(package.signature) << "\",\n";
    json << "  \"created_at\": \"" << jsonEscape(package.createdAt) << "\"\n";
    json << "}\n";
    return json.str();
}

bool sncFriendAcceptanceMatchesRequest(
    const SncFriendRequestPackage& request,
    const SncFriendAcceptancePackage& acceptance)
{
    return request.ok
        && acceptance.ok
        && acceptance.requestNodeId == request.sender.nodeId
        && acceptance.requestFingerprint == request.sender.fingerprint
        && acceptance.accepter.nodeId != request.sender.nodeId;
}

} // namespace strategic_nexus
