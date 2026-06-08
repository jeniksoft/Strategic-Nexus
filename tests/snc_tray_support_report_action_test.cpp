#include "SncTraySupportReportAction.h"

#include <cstdlib>
#include <iostream>

namespace {

void requireCondition(const bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << std::endl;
        std::exit(1);
    }
}

} // namespace

int main()
{
    {
        strategic_nexus::SncTraySupportReportActionStatus status;
        status.previewPathConfigured = false;
        status.previewReady = false;

        requireCondition(
            !strategic_nexus::sncTraySupportReportActionAvailable(status),
            "unconfigured support report path should disable action");
        requireCondition(
            strategic_nexus::buildSncTraySupportReportActionMenuLabel(status) ==
                L"Support report nedostupny",
            "unconfigured support report path should expose unavailable menu label");
        requireCondition(
            strategic_nexus::buildSncTraySupportReportActionButtonLabel(status) ==
                L"Report neni",
            "unconfigured support report path should expose unavailable button label");
    }

    {
        strategic_nexus::SncTraySupportReportActionStatus status;
        status.previewPathConfigured = true;
        status.previewReady = false;
        status.previewPath = "dist/private_reports/snc_support_report_preview.txt";

        requireCondition(
            strategic_nexus::sncTraySupportReportActionAvailable(status),
            "configured support report path should enable action");
        requireCondition(
            strategic_nexus::buildSncTraySupportReportActionMenuLabel(status) ==
                L"Pripravit support report",
            "missing preview should expose prepare menu label");
        requireCondition(
            strategic_nexus::buildSncTraySupportReportActionButtonLabel(status) ==
                L"Pripravit report",
            "missing preview should expose prepare button label");
    }

    {
        strategic_nexus::SncTraySupportReportActionStatus status;
        status.previewPathConfigured = true;
        status.previewReady = true;
        status.previewPath = "dist/private_reports/snc_support_report_preview.txt";

        requireCondition(
            strategic_nexus::sncTraySupportReportActionAvailable(status),
            "prepared support report preview should keep action enabled");
        requireCondition(
            strategic_nexus::buildSncTraySupportReportActionMenuLabel(status) ==
                L"Otevrit support report",
            "prepared preview should expose open menu label");
        requireCondition(
            strategic_nexus::buildSncTraySupportReportActionButtonLabel(status) ==
                L"Otevrit report",
            "prepared preview should expose open button label");
    }

    return 0;
}
