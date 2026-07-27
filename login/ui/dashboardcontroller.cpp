#include "dashboardcontroller.h"

#include "analytics/reader/activitylogreader.h"
#include "analytics/reader/networklogreader.h"
#include "analytics/reader/networkerrorreader.h"
#include "analytics/aggregator/activityaggregator.h"
#include "analytics/aggregator/networkaggregator.h"
#include "analytics/aggregator/appcorrelator.h"
#include "analytics/aggregator/timelineaggregator.h"
#include "analytics/dashboard/dashboarddatamodel.h"
#include "analytics/dashboard/htmldashboardgenerator.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>

DashboardController::DashboardController(QObject *parent)
    : QObject(parent)
{
}

QString DashboardController::generateDashboard(const QString &username)
{
    if (username.isEmpty()) {
        return {};
    }

    // ── 1. Read logs ──────────────────────────────────────────────────────────
    Analytics::ActivityLogReader  actReader;
    Analytics::NetworkLogReader   netReader;
    Analytics::NetworkErrorReader errReader;

    const auto actRecords  = actReader.readLogs(username);
    const auto netRecords  = netReader.readLogs(username);
    const auto errRecords  = errReader.readErrorLog(username);
    const auto diagRecords = errReader.readDiagLog(username);

    // ── 2. Aggregate ──────────────────────────────────────────────────────────
    Analytics::ActivityAggregator actAgg;
    Analytics::NetworkAggregator  netAgg;
    Analytics::AppCorrelator      correlator;
    Analytics::TimelineAggregator timelineAgg(60); // 1-minute buckets

    const auto actByProcess  = actAgg.aggregateByProcess(actRecords);
    const auto actOverview   = actAgg.buildOverview(actRecords, actByProcess);

    const auto netByProcess  = netAgg.aggregateByProcess(netRecords);
    const auto netByRemote   = netAgg.aggregateByRemoteHost(netRecords);
    const auto netOverview   = netAgg.buildOverview(netRecords, netByProcess, netByRemote);

    const auto appUsage      = correlator.correlate(actByProcess, netByProcess);
    const auto timeline      = timelineAgg.aggregate(actRecords, netRecords);

    // ── 3. Build data quality summary ─────────────────────────────────────────
    Analytics::DataQualitySummary dq;
    dq.totalNetworkSessions = netRecords.size();
    for (const Analytics::NetworkRecord &r : netRecords) {
        if (r.remoteHostname.isEmpty())   dq.unknownHostSessions++;
        if (r.processName.isEmpty() || r.processName == QStringLiteral("unknown"))
            dq.unknownProcessSessions++;
        if (r.closeReason == QStringLiteral("app_shutdown")) dq.shutdownFlushedSessions++;
        if (r.hostnameConfidence == QStringLiteral("low") ||
            r.hostnameConfidence == QStringLiteral("none") ||
            r.hostnameConfidence.isEmpty())
            dq.lowConfidenceSessions++;
    }
    for (const Analytics::ErrorRecord &e : errRecords) {
        if (e.isDnsWarning) dq.dnsWarningCount++;
        if (e.isShutdownFlush) dq.shutdownFlushedSessions++;
    }
    dq.unrecognizedDnsEventCount = diagRecords.size();

    // ── 4. Assemble model ─────────────────────────────────────────────────────
    Analytics::DashboardDataModel model;
    model.username       = username;
    model.generatedAt    = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    model.activityOverview  = actOverview;
    model.activityByProcess = actByProcess;
    model.networkOverview   = netOverview;
    model.networkByProcess  = netByProcess;
    model.networkByRemote   = netByRemote;
    model.networkRecords    = netRecords;
    model.appUsage          = appUsage;
    model.timeline          = timeline;
    model.dataQuality       = dq;

    // ── 5. Generate HTML ──────────────────────────────────────────────────────
    Analytics::HtmlDashboardGenerator generator;
    return generator.generate(model, username);
}

void DashboardController::generateAndOpenDashboard(const QString &username)
{
    if (username.isEmpty()) {
        emit statusMessage(QStringLiteral("Dashboard: no user logged in."));
        return;
    }

    emit statusMessage(QStringLiteral("Generating dashboard..."));
    const QString outputPath = generateDashboard(username);

    if (outputPath.isEmpty()) {
        emit statusMessage(QStringLiteral("Dashboard: failed to write dashboard.html"));
        return;
    }

    const QUrl fileUrl = QUrl::fromLocalFile(QFileInfo(outputPath).absoluteFilePath());
    QDesktopServices::openUrl(fileUrl);
    emit statusMessage(QStringLiteral("Dashboard opened in browser."));
}
