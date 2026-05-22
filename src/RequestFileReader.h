#pragma once

#include "StrategicRequest.h"

#include <filesystem>

namespace strategic_nexus {

class RequestFileReader {
public:
    StrategicRequest read(const std::filesystem::path& requestPath) const;
};

} // namespace strategic_nexus
