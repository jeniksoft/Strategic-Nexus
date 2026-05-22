#include "SequenceStateStore.h"

#include "common/FileUtil.h"

#include <fstream>
#include <sstream>

namespace strategic_nexus::bridge_core {

SequenceReadResult SequenceStateStore::read(const std::filesystem::path& path) const
{
    SequenceReadResult result;

    if (path.empty()) {
        result.reason = "missing sequence state path";
        return result;
    }

    if (!std::filesystem::exists(path)) {
        result.ok = true;
        result.reason = "missing state, starting at zero";
        result.lastAcceptedSequenceId = 0;
        return result;
    }

    std::ifstream input(path);
    if (!input) {
        result.reason = "failed to open sequence state";
        return result;
    }

    std::string text;
    std::getline(input, text);
    std::istringstream parser(text);
    std::int64_t value = 0;
    parser >> value;
    if (!parser || value < 0) {
        result.reason = "invalid sequence state";
        return result;
    }

    result.ok = true;
    result.reason = "read";
    result.lastAcceptedSequenceId = value;
    return result;
}

SequenceWriteResult SequenceStateStore::writeAtomically(
    const std::filesystem::path& path,
    const std::int64_t sequenceId) const
{
    SequenceWriteResult result;

    if (path.empty()) {
        result.reason = "missing sequence state path";
        return result;
    }

    if (sequenceId < 0) {
        result.reason = "invalid sequence id";
        return result;
    }

    std::ostringstream text;
    text << sequenceId << '\n';
    if (!common::writeTextFileAtomically(path, text.str())) {
        result.reason = "failed to replace sequence state";
        return result;
    }

    result.written = true;
    result.reason = "written";
    return result;
}

} // namespace strategic_nexus::bridge_core
