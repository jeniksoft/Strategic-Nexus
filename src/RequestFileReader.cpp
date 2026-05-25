// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "RequestFileReader.h"

#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace strategic_nexus {

namespace {

std::string readTextFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to open request file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::optional<std::string> readJsonStringField(const std::string& text, const std::string& fieldName)
{
    const std::string quotedField = "\"" + fieldName + "\"";
    const std::size_t fieldPosition = text.find(quotedField);
    if (fieldPosition == std::string::npos) {
        return std::nullopt;
    }

    const std::size_t colonPosition = text.find(':', fieldPosition + quotedField.size());
    if (colonPosition == std::string::npos) {
        return std::nullopt;
    }

    const std::size_t valueStart = text.find('"', colonPosition + 1);
    if (valueStart == std::string::npos) {
        return std::nullopt;
    }

    const std::size_t valueEnd = text.find('"', valueStart + 1);
    if (valueEnd == std::string::npos) {
        return std::nullopt;
    }

    return text.substr(valueStart + 1, valueEnd - valueStart - 1);
}

std::optional<bool> readJsonBoolField(const std::string& text, const std::string& fieldName)
{
    const std::string quotedField = "\"" + fieldName + "\"";
    const std::size_t fieldPosition = text.find(quotedField);
    if (fieldPosition == std::string::npos) {
        return std::nullopt;
    }

    const std::size_t colonPosition = text.find(':', fieldPosition + quotedField.size());
    if (colonPosition == std::string::npos) {
        return std::nullopt;
    }

    const std::size_t valueStart = text.find_first_not_of(" \t\r\n", colonPosition + 1);
    if (valueStart == std::string::npos) {
        return std::nullopt;
    }

    if (text.compare(valueStart, 4, "true") == 0) {
        return true;
    }
    if (text.compare(valueStart, 5, "false") == 0) {
        return false;
    }

    return std::nullopt;
}

void validateJsonEnvelope(const std::string& text)
{
    const std::size_t firstContent = text.find_first_not_of(" \t\r\n");
    const std::size_t lastContent = text.find_last_not_of(" \t\r\n");

    if (firstContent == std::string::npos || lastContent == std::string::npos) {
        throw std::runtime_error("Request file is empty.");
    }

    if (text[firstContent] != '{' || text[lastContent] != '}') {
        throw std::runtime_error("Request file is incomplete or malformed.");
    }
}

} // namespace

StrategicRequest RequestFileReader::read(const std::filesystem::path& requestPath) const
{
    const std::string text = readTextFile(requestPath);
    validateJsonEnvelope(text);

    StrategicRequest request;

    if (const auto requestId = readJsonStringField(text, "request_id")) {
        request.requestId = *requestId;
    }

    if (const auto trigger = readJsonStringField(text, "trigger")) {
        request.trigger = requestTriggerFromString(*trigger);
    }

    if (const auto snapshotPath = readJsonStringField(text, "snapshot_path")) {
        request.snapshotPath = *snapshotPath;
    }

    if (const auto outputDoctrinePath = readJsonStringField(text, "output_doctrine_path")) {
        request.outputDoctrinePath = *outputDoctrinePath;
    }

    if (const auto asynchronous = readJsonBoolField(text, "async")) {
        request.asynchronous = *asynchronous;
    }

    return request;
}

} // namespace strategic_nexus

