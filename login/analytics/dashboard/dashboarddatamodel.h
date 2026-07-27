#ifndef ANALYTICS_DASHBOARD_DASHBOARDDATAMODEL_H
#define ANALYTICS_DASHBOARD_DASHBOARDDATAMODEL_H

#include "analytics/model/activitysummary.h"
#include "analytics/model/networksummary.h"
#include "analytics/model/appsummary.h"
#include "analytics/model/timelinebucket.h"
#include "analytics/model/analyticsmodels.h"

#include <QList>
#include <QString>
#include <QJsonObject>

namespace Analytics {

// Aggregated data quality indicators for the dashboard Data Quality section.
struct DataQualitySummary
{
    int totalNetworkSessions      = 0;
    int unknownHostSessions       = 0;
    int unknownProcessSessions    = 0;
    int shutdownFlushedSessions   = 0;
    int dnsWarningCount           = 0;
    int unrecognizedDnsEventCount = 0; // from network_dns_diag.jsonl
    int lowConfidenceSessions     = 0; // hostname_confidence == "low" || "none"

    double unknownHostPct() const {
        return totalNetworkSessions > 0
            ? 100.0 * unknownHostSessions / totalNetworkSessions : 0.0;
    }
    double unknownProcessPct() const {
        return totalNetworkSessions > 0
            ? 100.0 * unknownProcessSessions / totalNetworkSessions : 0.0;
    }
};

// The complete model passed to HtmlDashboardGenerator.
struct DashboardDataModel
{
    QString username;
    QString generatedAt; // ISO timestamp

    // Activity section
    ActivityOverview           activityOverview;
    QList<ActivitySummary>     activityByProcess;

    // Network section
    NetworkOverview            networkOverview;
    QList<NetworkProcessSummary> networkByProcess;
    QList<NetworkRemoteSummary>  networkByRemote;
    QList<NetworkRecord>         networkRecords; // for detailed table

    // App usage section
    QList<AppSummary>          appUsage;

    // Timeline section
    QList<TimelineBucket>      timeline;

    // Data quality section
    DataQualitySummary         dataQuality;
};

} // namespace Analytics

#endif // ANALYTICS_DASHBOARD_DASHBOARDDATAMODEL_H
