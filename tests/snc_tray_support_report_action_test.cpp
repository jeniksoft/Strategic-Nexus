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
                L"Diagnosticka zprava neni dostupna",
            "unconfigured support report path should expose unavailable menu label");
        requireCondition(
            strategic_nexus::buildSncTraySupportReportActionButtonLabel(status) ==
                L"Zprava neni dostupna",
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
                L"Pripravit diagnostickou zpravu",
            "missing preview should expose prepare menu label");
        requireCondition(
            strategic_nexus::buildSncTraySupportReportActionButtonLabel(status) ==
                L"Pripravit zpravu",
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
                L"Otevrit diagnostickou zpravu",
            "prepared preview should expose open menu label");
        requireCondition(
            strategic_nexus::buildSncTraySupportReportActionButtonLabel(status) ==
                L"Otevrit zpravu",
            "prepared preview should expose open button label");
    }

    return 0;
}
