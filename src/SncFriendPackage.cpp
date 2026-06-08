// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SncFriendPackage.h"

#include "common/JsonExtract.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <regex>
#include <sstream>

namespace strategic_nexus {
namespace {

constexpr std::size_t kMaxNodeIdLength = 80;
constexpr std::size_t kMaxDisplayNameLength = 80;
constexpr std::size_t kMaxPublicKeyLength = 180;
constexpr std::size_t kMaxFingerprintLength = 96;

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
