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
        return L"Diagnosticka zprava neni dostupna";
    }
    return status.previewReady ? L"Otevrit diagnostickou zpravu" : L"Pripravit diagnostickou zpravu";
}

inline std::wstring buildSncTraySupportReportActionButtonLabel(
    const SncTraySupportReportActionStatus& status)
{
    if (!sncTraySupportReportActionAvailable(status)) {
        return L"Zprava neni dostupna";
    }
    return status.previewReady ? L"Otevrit zpravu" : L"Pripravit zpravu";
}

} // namespace strategic_nexus
