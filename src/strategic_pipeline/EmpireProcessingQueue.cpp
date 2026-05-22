#include "EmpireProcessingQueue.h"

#include <algorithm>

namespace strategic_nexus::strategic_pipeline {

std::vector<EmpireProcessingQueueEntry> EmpireProcessingQueue::build(std::vector<MinistryInputContext> inputs) const
{
    ProcessingPriorityScorer scorer;
    std::vector<EmpireProcessingQueueEntry> entries;
    entries.reserve(inputs.size());

    for (std::size_t index = 0; index < inputs.size(); ++index) {
        EmpireProcessingQueueEntry entry;
        entry.input = std::move(inputs[index]);
        entry.priorityScore = scorer.score(entry.input);
        entry.originalIndex = index;
        entries.push_back(std::move(entry));
    }

    std::stable_sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
        return left.priorityScore.totalPriority > right.priorityScore.totalPriority;
    });

    return entries;
}

} // namespace strategic_nexus::strategic_pipeline
