#pragma once

#include <filesystem>
#include <string>

namespace strategic_nexus {

struct SncTraySupportReportActionStatus {
    bool previewPathConfigured = false;
    bool previewReady = false;
    std::filesystem::path previewPath;
};

inline bool sncTraySupportReportActionAvailable(const SncTraySupportReportActionStatus& status)
{
    return status.previewPathConfigured;
}

inline std::wstring buildSncTraySupportReportActionMenuLabel(
    const SncTraySupportReportActionStatus& status)
{
    if (!sncTraySupportReportActionAvailable(status)) {
        return L"Support report nedostupny";
    }
    return status.previewReady ? L"Otevrit support report" : L"Pripravit support report";
}

inline std::wstring buildSncTraySupportReportActionButtonLabel(
    const SncTraySupportReportActionStatus& status)
{
    if (!sncTraySupportReportActionAvailable(status)) {
        return L"Report neni";
    }
    return status.previewReady ? L"Otevrit report" : L"Pripravit report";
}

} // namespace strategic_nexus
