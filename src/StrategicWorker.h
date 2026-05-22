#pragma once

#include "StrategicRequest.h"

namespace strategic_nexus {

class StrategicWorker {
public:
    int processRequest(const StrategicRequest& request) const;
    int processRequestSafely(const StrategicRequest& request) const;
};

} // namespace strategic_nexus
