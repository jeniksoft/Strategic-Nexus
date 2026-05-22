#pragma once

#include "CoreTypes.h"

#include <filesystem>

namespace strategic_nexus {

class SaveParser {
public:
    GalaxyState parseSnapshot(const std::filesystem::path& savePath) const;
};

} // namespace strategic_nexus
