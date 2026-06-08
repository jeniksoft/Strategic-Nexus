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
