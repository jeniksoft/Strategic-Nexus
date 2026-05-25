// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "JsonExtract.h"

#include <regex>

namespace strategic_nexus::common {
namespace {

bool isHexDigit(const char character)
{
    return (character >= '0' && character <= '9')
        || (character >= 'a' && character <= 'f')
        || (character >= 'A' && character <= 'F');
}

std::optional<std::string> findJsonArrayBody(const std::string& json, const char* key)
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
        const char character = json[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                inString = false;
            }
            continue;
        }

        if (character == '"') {
            inString = true;
            continue;
        }
        if (character == '[') {
            ++depth;
            continue;
        }
        if (character == ']') {
            --depth;
            if (depth == 0) {
                return json.substr(bodyStart, index - bodyStart);
            }
        }
    }

    return std::nullopt;
}

} // namespace

std::string decodeJsonStringLiteral(const std::string& rawLiteral)
{
    std::string decoded;
    decoded.reserve(rawLiteral.size());

    for (std::size_t index = 0; index < rawLiteral.size(); ++index) {
        const char character = rawLiteral[index];
        if (character != '\\' || index + 1 >= rawLiteral.size()) {
            decoded.push_back(character);
            continue;
        }

        const char escaped = rawLiteral[++index];
        switch (escaped) {
        case '"': decoded.push_back('"'); break;
        case '\\': decoded.push_back('\\'); break;
        case '/': decoded.push_back('/'); break;
        case 'b': decoded.push_back('\b'); break;
        case 'f': decoded.push_back('\f'); break;
        case 'n': decoded.push_back('\n'); break;
        case 'r': decoded.push_back('\r'); break;
        case 't': decoded.push_back('\t'); break;
        case 'u':
            if (index + 4 < rawLiteral.size()
                && isHexDigit(rawLiteral[index + 1])
                && isHexDigit(rawLiteral[index + 2])
                && isHexDigit(rawLiteral[index + 3])
                && isHexDigit(rawLiteral[index + 4])) {
                decoded.push_back('?');
                index += 4;
            } else {
                decoded.push_back('u');
            }
            break;
        default:
            decoded.push_back(escaped);
            break;
        }
    }

    return decoded;
}

std::optional<std::string> extractJsonString(const std::string& json, const char* key)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*\"((?:\\\\.|[^\"\\\\])*)\"");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return std::nullopt;
    }

    return decodeJsonStringLiteral(match[1].str());
}

std::vector<std::string> extractJsonStringArray(const std::string& json, const char* key)
{
    std::vector<std::string> values;
    const std::optional<std::string> arrayBody = findJsonArrayBody(json, key);
    if (!arrayBody.has_value()) {
        return values;
    }

    const std::regex stringPattern("\"((?:\\\\.|[^\"\\\\])*)\"");
    auto begin = std::sregex_iterator(arrayBody->begin(), arrayBody->end(), stringPattern);
    const auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        values.push_back(decodeJsonStringLiteral((*it)[1].str()));
    }

    return values;
}

} // namespace strategic_nexus::common

