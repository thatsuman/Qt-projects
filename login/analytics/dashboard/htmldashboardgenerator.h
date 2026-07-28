#ifndef ANALYTICS_DASHBOARD_HTMLDASHBOARDGENERATOR_H
#define ANALYTICS_DASHBOARD_HTMLDASHBOARDGENERATOR_H

#include "analytics/dashboard/dashboarddatamodel.h"

#include <QString>

namespace Analytics {

// Generates a self-contained dashboard.html from a DashboardDataModel.
// No external CDN, no web server, no QtWebEngine — output is openable
// in any modern browser via the file:// protocol.
class HtmlDashboardGenerator
{
public:
    HtmlDashboardGenerator() = default;

    // Returns the output path on success, or empty string on failure.
    QString generate(const DashboardDataModel &model, const QString &username);

private:
    QString buildHtml(const DashboardDataModel &model) const;
    QString buildDataScript(const DashboardDataModel &model) const;
    QString buildOverviewCards(const DashboardDataModel &model) const;
    QString buildDataQualityRows(const DataQualitySummary &dq) const;

    static QString fmtBytes(quint64 bytes);
    static QString fmtSecs(qint64 secs);
};

} // namespace Analytics

#endif // ANALYTICS_DASHBOARD_HTMLDASHBOARDGENERATOR_H
