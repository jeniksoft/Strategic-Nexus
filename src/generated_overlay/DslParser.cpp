// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "DslParser.h"

#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace strategic_nexus::generated_overlay {
namespace {

struct Token {
    std::string text;
    bool quoted = false;
};

std::vector<Token> tokenize(const std::string& text, std::string& error)
{
    std::vector<Token> tokens;

    for (std::size_t index = 0; index < text.size();) {
        if (index == 0 &&
            text.size() >= 3 &&
            static_cast<unsigned char>(text[0]) == 0xEF &&
            static_cast<unsigned char>(text[1]) == 0xBB &&
            static_cast<unsigned char>(text[2]) == 0xBF) {
            index = 3;
            continue;
        }

        const char ch = text[index];

        if (std::isspace(static_cast<unsigned char>(ch))) {
            ++index;
            continue;
        }

        if (ch == '#') {
            while (index < text.size() && text[index] != '\n') {
                ++index;
            }
            continue;
        }

        if (ch == '{' || ch == '}' || ch == '=') {
            tokens.push_back({std::string(1, ch), false});
            ++index;
            continue;
        }

        if (ch == '>' || ch == '<') {
            std::string op(1, ch);
            ++index;
            if (index < text.size() && text[index] == '=') {
                op.push_back('=');
                ++index;
            }
            tokens.push_back({op, false});
            continue;
        }

        if (ch == '"') {
            ++index;
            std::string value;
            while (index < text.size() && text[index] != '"') {
                if (text[index] == '\\') {
                    error = "backslash escapes are not supported in DSL strings";
                    return {};
                }
                const unsigned char quotedChar = static_cast<unsigned char>(text[index]);
                if (std::iscntrl(quotedChar)) {
                    error = "control characters are not supported in DSL strings";
                    return {};
                }
                value.push_back(text[index]);
                ++index;
            }

            if (index >= text.size()) {
                error = "unterminated quoted string";
                return {};
            }

            ++index;
            tokens.push_back({value, true});
            continue;
        }

        std::string value;
        while (index < text.size()) {
            const char current = text[index];
            if (std::isspace(static_cast<unsigned char>(current)) ||
                current == '{' || current == '}' || current == '=' ||
                current == '>' || current == '<' || current == '"') {
                break;
            }
            value.push_back(current);
            ++index;
        }

        if (value.empty()) {
            error = "unexpected character in DSL";
            return {};
        }

        tokens.push_back({value, false});
    }

    return tokens;
}

class ParserCursor {
public:
    explicit ParserCursor(std::vector<Token> tokens)
        : tokens_(std::move(tokens))
    {
    }

    bool done() const
    {
        return position_ >= tokens_.size();
    }

    bool consume(const std::string& expected)
    {
        if (done() || tokens_[position_].text != expected) {
            return false;
        }
        ++position_;
        return true;
    }

    bool readAny(std::string& value)
    {
        if (done()) {
            return false;
        }
        value = tokens_[position_].text;
        ++position_;
        return true;
    }

    bool readQuoted(std::string& value)
    {
        if (done() || !tokens_[position_].quoted) {
            return false;
        }
        value = tokens_[position_].text;
        ++position_;
        return true;
    }

    std::string peek() const
    {
        return done() ? "" : tokens_[position_].text;
    }

private:
    std::vector<Token> tokens_;
    std::size_t position_ = 0;
};

bool readNumber(const std::string& text, double& value)
{
    char* end = nullptr;
    value = std::strtod(text.c_str(), &end);
    return end != text.c_str() && *end == '\0';
}

bool readAssignmentValue(ParserCursor& cursor, const std::string& key, std::string& value, std::string& error)
{
    const std::string next = cursor.peek();
    if (next.empty() || next == "=" || next == "}" ||
        next == "ministry" || next == "duration" || next == "confidence" || next == "rationale" ||
        next == "prefer" || next == "when" || next == "rule" || next == "empire" || next == "campaign") {
        error = key + " assignment requires value";
        return false;
    }
    return cursor.readAny(value);
}

bool parsePreference(ParserCursor& cursor, DslRule& rule, std::string& error)
{
    DslPreference preference;
    if (!cursor.readAny(preference.domain) || !cursor.readAny(preference.value)) {
        error = "prefer requires domain and value";
        return false;
    }

    if (cursor.peek() == "intensity") {
        std::string intensityText;
        cursor.consume("intensity");
        if (!cursor.readAny(intensityText) || !readNumber(intensityText, preference.intensity)) {
            error = "intensity must be numeric";
            return false;
        }
    }

    rule.preferences.push_back(std::move(preference));
    return true;
}

bool parseCondition(ParserCursor& cursor, DslRule& rule, std::string& error)
{
    DslCondition condition;
    if (!cursor.readAny(condition.source)) {
        error = "when requires source";
        return false;
    }

    if (cursor.peek() == ">=" || cursor.peek() == "<=" || cursor.peek() == "=") {
        cursor.readAny(condition.op);
        if (!cursor.readAny(condition.value)) {
            error = "when comparison requires value";
            return false;
        }
    }

    rule.conditions.push_back(std::move(condition));
    return true;
}

bool parseAssignment(ParserCursor& cursor, DslRule& rule, std::string& error)
{
    std::string key;
    if (!cursor.readAny(key) || !cursor.consume("=")) {
        error = "assignment must use key = value";
        return false;
    }

    if (key == "ministry") {
        if (!readAssignmentValue(cursor, key, rule.ministry, error)) {
            return false;
        }
        return true;
    }
    if (key == "duration") {
        if (!readAssignmentValue(cursor, key, rule.duration, error)) {
            return false;
        }
        return true;
    }
    if (key == "confidence") {
        std::string confidenceText;
        if (!cursor.readAny(confidenceText) || !readNumber(confidenceText, rule.confidence)) {
            error = "confidence must be numeric";
            return false;
        }
        return true;
    }
    if (key == "rationale") {
        if (!cursor.readQuoted(rule.rationale)) {
            error = "rationale must be a quoted string";
            return false;
        }
        return true;
    }

    error = "unknown rule assignment: " + key;
    return false;
}

bool parseRule(ParserCursor& cursor, const std::string& campaignId, const std::string& empireId, DslProgram& program, std::string& error)
{
    DslRule rule;
    rule.campaignId = campaignId;
    rule.empireId = empireId;

    if (!cursor.consume("rule") || !cursor.readQuoted(rule.ruleId) || !cursor.consume("{")) {
        error = "expected rule \"id\" {";
        return false;
    }

    while (!cursor.done() && cursor.peek() != "}") {
        if (cursor.peek() == "prefer") {
            cursor.consume("prefer");
            if (!parsePreference(cursor, rule, error)) {
                return false;
            }
            continue;
        }

        if (cursor.peek() == "when") {
            cursor.consume("when");
            if (!parseCondition(cursor, rule, error)) {
                return false;
            }
            continue;
        }

        if (!parseAssignment(cursor, rule, error)) {
            return false;
        }
    }

    if (!cursor.consume("}")) {
        error = "unterminated rule block";
        return false;
    }

    program.rules.push_back(std::move(rule));
    return true;
}

bool parseEmpire(ParserCursor& cursor, const std::string& campaignId, DslProgram& program, std::string& error)
{
    std::string empireId;
    if (!cursor.consume("empire") || !cursor.readQuoted(empireId) || !cursor.consume("{")) {
        error = "expected empire \"id\" {";
        return false;
    }

    while (!cursor.done() && cursor.peek() != "}") {
        if (!parseRule(cursor, campaignId, empireId, program, error)) {
            return false;
        }
    }

    if (!cursor.consume("}")) {
        error = "unterminated empire block";
        return false;
    }

    return true;
}

bool parseCampaign(ParserCursor& cursor, DslProgram& program, std::string& error)
{
    std::string campaignId;
    if (!cursor.consume("campaign") || !cursor.readQuoted(campaignId) || !cursor.consume("{")) {
        error = "expected campaign \"id\" {";
        return false;
    }

    while (!cursor.done() && cursor.peek() != "}") {
        if (!parseEmpire(cursor, campaignId, program, error)) {
            return false;
        }
    }

    if (!cursor.consume("}")) {
        error = "unterminated campaign block";
        return false;
    }

    return true;
}

} // namespace

DslParseResult DslParser::parse(const std::string& text) const
{
    std::string error;
    auto tokens = tokenize(text, error);
    if (!error.empty()) {
        return {false, error, {}};
    }

    ParserCursor cursor(std::move(tokens));
    DslProgram program;

    while (!cursor.done()) {
        if (!parseCampaign(cursor, program, error)) {
            return {false, error, {}};
        }
    }

    return {true, "", std::move(program)};
}

} // namespace strategic_nexus::generated_overlay

