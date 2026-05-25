// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "MemoryStore.h"

namespace strategic_nexus {

void MemoryStore::ingest(const std::vector<HistoricalEvent>& events)
{
    events_.insert(events_.end(), events.begin(), events.end());
}

const std::vector<HistoricalEvent>& MemoryStore::events() const
{
    return events_;
}

} // namespace strategic_nexus

