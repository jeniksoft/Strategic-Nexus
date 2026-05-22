#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace strategic_nexus::bridge_core {

struct SequenceReadResult {
    bool ok = false;
    std::int64_t lastAcceptedSequenceId = 0;
    std::string reason;
};

struct SequenceWriteResult {
    bool written = false;
    std::string reason;
};

class SequenceStateStore {
public:
    SequenceReadResult read(const std::filesystem::path& path) const;
    SequenceWriteResult writeAtomically(const std::filesystem::path& path, std::int64_t sequenceId) const;
};

} // namespace strategic_nexus::bridge_core
