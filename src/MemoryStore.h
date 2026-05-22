#pragma once

#include "CoreTypes.h"

#include <vector>

namespace strategic_nexus {

class MemoryStore {
public:
    void ingest(const std::vector<HistoricalEvent>& events);
    const std::vector<HistoricalEvent>& events() const;

private:
    std::vector<HistoricalEvent> events_;
};

} // namespace strategic_nexus
