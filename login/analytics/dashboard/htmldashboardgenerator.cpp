#include "htmldashboardgenerator.h"
#include "dashboardtemplate.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace Analytics {

// ── Public API ────────────────────────────────────────────────────────────────

QString HtmlDashboardGenerator::generate(const DashboardDataModel &model, const QString &username)
{
    const QString dir  = QString("logs/%1").arg(username);
    const QString path = dir + "/dashboard.html";

    QDir().mkpath(dir);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return {};
    }

    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#else
    out.setEncoding(QStringConverter::Utf8);
#endif
    out << buildHtml(model);
    file.close();
    return path;
}

// ── HTML builder ──────────────────────────────────────────────────────────────

QString HtmlDashboardGenerator::buildHtml(const DashboardDataModel &model) const
{
    const QString tabs = QStringLiteral(
        "<button class='active' data-tab='overview'     onclick='showTab(\"overview\")'    >Overview</button>"
        "<button data-tab='activity'    onclick='showTab(\"activity\")'   >Activity</button>"
        "<button data-tab='network'     onclick='showTab(\"network\")'    >Network</button>"
        "<button data-tab='appusage'    onclick='showTab(\"appusage\")'   >App Usage</button>"
        "<button data-tab='timeline'    onclick='showTab(\"timeline\")'   >Timeline</button>"
        "<button data-tab='dataquality' onclick='showTab(\"dataquality\")'>Data Quality</button>"
    );

    // --- Overview section ---
    const QString overviewSection = QStringLiteral(
        "<div id='overview' class='tab-content active'>"
        "<h2>Session Overview</h2>"
        "<div class='cards'>%1</div>"
        "</div>"
    ).arg(buildOverviewCards(model));

    // --- Activity section ---
    const QString activitySection = QStringLiteral(
        "<div id='activity' class='tab-content'>"
        "<h2>Activity by Application</h2>"
        "<div class='table-wrap'>"
        "  <div class='table-toolbar'><input id='act-filter' placeholder='Filter apps...'></div>"
        "  <table id='act-table'><thead><tr>"
        "    <th data-col='0'>Process</th>"
        "    <th data-col='1'>Active Time</th>"
        "    <th data-col='2'>Sessions</th>"
        "    <th data-col='3'>Keystrokes</th>"
        "    <th data-col='4'>Mouse Distance</th>"
        "  </tr></thead><tbody></tbody></table>"
        "</div>"
        "<p class='note'>Keystroke count only — raw content not stored or displayed.</p>"
        "</div>"
    );

    // --- Network section ---
    const QString networkSection = QStringLiteral(
        "<div id='network' class='tab-content'>"
        "<h2>Network by Application</h2>"
        "<div class='table-wrap'>"
        "  <div class='table-toolbar'><input id='net-proc-filter' placeholder='Filter apps...'></div>"
        "  <table id='net-proc-table'><thead><tr>"
        "    <th data-col='0'>Process</th>"
        "    <th data-col='1'>Sent</th>"
        "    <th data-col='2'>Received</th>"
        "    <th data-col='3'>Total</th>"
        "    <th data-col='4'>Sessions</th>"
        "    <th data-col='5'>Protocols</th>"
        "  </tr></thead><tbody></tbody></table>"
        "</div>"
        "<h2>Network Session Records</h2>"
        "<div class='table-wrap'>"
        "  <div class='table-toolbar'><input id='net-rec-filter' placeholder='Filter records...'></div>"
        "  <table id='net-rec-table'><thead><tr>"
        "    <th data-col='0'>Process</th>"
        "    <th data-col='1'>Remote Host</th>"
        "    <th data-col='2'>Port</th>"
        "    <th data-col='3'>Protocol</th>"
        "    <th data-col='4'>Sent</th>"
        "    <th data-col='5'>Received</th>"
        "    <th data-col='6'>Host Confidence</th>"
        "    <th data-col='7'>Close Reason</th>"
        "  </tr></thead><tbody></tbody></table>"
        "</div>"
        "</div>"
    );

    // --- App Usage section ---
    const QString appUsageSection = QStringLiteral(
        "<div id='appusage' class='tab-content'>"
        "<h2>App Usage — Activity + Network Combined</h2>"
        "<div class='table-wrap'>"
        "  <div class='table-toolbar'><input id='app-filter' placeholder='Filter apps...'></div>"
        "  <table id='app-table'><thead><tr>"
        "    <th data-col='0'>Process</th>"
        "    <th data-col='1'>Active Time</th>"
        "    <th data-col='2'>Keystrokes</th>"
        "    <th data-col='3'>Sent</th>"
        "    <th data-col='4'>Received</th>"
        "    <th data-col='5'>Correlation</th>"
        "  </tr></thead><tbody></tbody></table>"
        "</div>"
        "<p class='note'>Correlation is approximate. Activity and network data are matched by process name and are not causally linked.</p>"
        "</div>"
    );

    // --- Timeline section ---
    const QString timelineSection = QStringLiteral(
        "<div id='timeline' class='tab-content'>"
        "<h2>Timeline (1-minute buckets)</h2>"
        "<div class='table-wrap' style='padding:16px'>"
        "  <p class='note' style='margin-bottom:12px'>Blue = foreground activity time &nbsp;|&nbsp; Teal = network traffic volume</p>"
        "  <div class='timeline-wrap'><div class='timeline-grid' id='tl-grid'></div></div>"
        "</div>"
        "</div>"
    );

    // --- Data Quality section ---
    const QString dqSection = QStringLiteral(
        "<div id='dataquality' class='tab-content'>"
        "<h2>Data Quality</h2>"
        "<div class='table-wrap'>%1</div>"
        "</div>"
    ).arg(buildDataQualityRows(model.dataQuality));

    return QStringLiteral(
        "<!DOCTYPE html>"
        "<html lang='en'><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<title>Session Dashboard — %1</title>"
        "<style>%2</style>"
        "</head><body>"
        "<header><h1>&#128202; Session Dashboard</h1>"
        "<div class='meta'>User: <strong>%1</strong> &nbsp;|&nbsp; Generated: %3</div>"
        "</header>"
        "<nav>%4</nav>"
        "%5%6%7%8%9%10"
        "<script>%11\n%12</script>"
        "</body></html>"
    )
    .arg(model.username.toHtmlEscaped())            // %1
    .arg(dashboardCss())                          // %2
    .arg(model.generatedAt)                       // %3
    .arg(tabs)                                    // %4
    .arg(overviewSection)                         // %5
    .arg(activitySection)                         // %6
    .arg(networkSection)                          // %7
    .arg(appUsageSection)                         // %8
    .arg(timelineSection)                         // %9
    .arg(dqSection)                               // %10
    .arg(buildDataScript(model))                  // %11
    .arg(dashboardJs());                          // %12
}

// ── Data serialisation ────────────────────────────────────────────────────────

QString HtmlDashboardGenerator::buildDataScript(const DashboardDataModel &model) const
{
    QJsonObject root;

    // Activity by process
    QJsonArray actArr;
    for (const ActivitySummary &s : model.activityByProcess) {
        QJsonObject o;
        o[QStringLiteral("processName")]             = s.processName;
        o[QStringLiteral("totalActiveDurationSeconds")] = s.totalActiveDurationSeconds;
        o[QStringLiteral("sessionCount")]            = s.sessionCount;
        o[QStringLiteral("totalKeystrokes")]         = s.totalKeystrokes;
        o[QStringLiteral("totalMouseDistancePx")]    = s.totalMouseDistancePx;
        actArr.append(o);
    }
    root[QStringLiteral("activityByProcess")] = actArr;

    // Network by process
    QJsonArray netProcArr;
    for (const NetworkProcessSummary &s : model.networkByProcess) {
        QJsonObject o;
        o[QStringLiteral("processName")]   = s.processName;
        o[QStringLiteral("bytesSent")]     = static_cast<double>(s.bytesSent);
        o[QStringLiteral("bytesReceived")] = static_cast<double>(s.bytesReceived);
        o[QStringLiteral("totalBytes")]    = static_cast<double>(s.totalBytes);
        o[QStringLiteral("sessionCount")]  = s.sessionCount;
        QJsonArray ph;
        for (const QString &p : s.protocolHints) ph.append(p);
        o[QStringLiteral("protocolHints")] = ph;
        netProcArr.append(o);
    }
    root[QStringLiteral("networkByProcess")] = netProcArr;

    // Network records (detailed)
    QJsonArray netRecArr;
    for (const NetworkRecord &r : model.networkRecords) {
        QJsonObject o;
        o[QStringLiteral("processName")]        = r.processName;
        o[QStringLiteral("remoteHostname")]      = r.remoteHostname;
        o[QStringLiteral("remoteIp")]            = r.remoteIp;
        o[QStringLiteral("remotePort")]          = r.remotePort;
        o[QStringLiteral("appProtocolHint")]     = r.appProtocolHint;
        o[QStringLiteral("transportProtocol")]   = r.transportProtocol;
        o[QStringLiteral("bytesSent")]           = static_cast<double>(r.bytesSent);
        o[QStringLiteral("bytesReceived")]       = static_cast<double>(r.bytesReceived);
        o[QStringLiteral("hostnameConfidence")]  = r.hostnameConfidence;
        o[QStringLiteral("closeReason")]         = r.closeReason;
        netRecArr.append(o);
    }
    root[QStringLiteral("networkRecords")] = netRecArr;

    // App usage
    QJsonArray appArr;
    for (const AppSummary &s : model.appUsage) {
        QJsonObject o;
        o[QStringLiteral("processName")]           = s.processName;
        o[QStringLiteral("activeDurationSeconds")]  = s.activeDurationSeconds;
        o[QStringLiteral("keystrokeCount")]         = s.keystrokeCount;
        o[QStringLiteral("bytesSent")]              = static_cast<double>(s.bytesSent);
        o[QStringLiteral("bytesReceived")]          = static_cast<double>(s.bytesReceived);
        o[QStringLiteral("correlationConfidence")]  = s.correlationConfidence;
        appArr.append(o);
    }
    root[QStringLiteral("appUsage")] = appArr;

    // Timeline buckets
    QJsonArray tlArr;
    for (const TimelineBucket &b : model.timeline) {
        QJsonObject o;
        o[QStringLiteral("label")]            = b.bucketStart.toString(QStringLiteral("HH:mm"));
        o[QStringLiteral("foregroundProcess")] = b.foregroundProcess;
        o[QStringLiteral("activeSeconds")]     = b.activeSeconds;
        o[QStringLiteral("bytesSent")]         = static_cast<double>(b.bytesSent);
        o[QStringLiteral("bytesReceived")]     = static_cast<double>(b.bytesReceived);
        o[QStringLiteral("totalBytes")]        = static_cast<double>(b.totalBytes);
        tlArr.append(o);
    }
    root[QStringLiteral("timeline")] = tlArr;

    QString jsonStr = QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
    jsonStr.replace(QLatin1Char('<'), QStringLiteral("\\u003c"));
    jsonStr.replace(QLatin1Char('>'), QStringLiteral("\\u003e"));
    return QStringLiteral("const DASHBOARD_DATA = %1;").arg(jsonStr);
}

// ── Overview cards ────────────────────────────────────────────────────────────

QString HtmlDashboardGenerator::buildOverviewCards(const DashboardDataModel &model) const
{
    auto card = [](const QString &label, const QString &value, const QString &sub = {}) -> QString {
        return QStringLiteral(
            "<div class='card'>"
            "<div class='label'>%1</div>"
            "<div class='value'>%2</div>"
            "%3"
            "</div>"
        ).arg(label, value, sub.isEmpty() ? QString() : QStringLiteral("<div class='sub'>%1</div>").arg(sub));
    };

    const auto &ov = model.activityOverview;
    const auto &nv = model.networkOverview;

    return card(QStringLiteral("Session Duration"), fmtSecs(ov.totalSessionSeconds))
         + card(QStringLiteral("Active Time"),      fmtSecs(ov.totalActiveSeconds))
         + card(QStringLiteral("Total Sent"),       fmtBytes(nv.totalBytesSent))
         + card(QStringLiteral("Total Received"),   fmtBytes(nv.totalBytesReceived))
         + card(QStringLiteral("Top Active App"),   ov.topActiveProcess.isEmpty() ? QStringLiteral("-") : ov.topActiveProcess.toHtmlEscaped())
         + card(QStringLiteral("Top Network App"),  nv.topNetworkProcess.isEmpty() ? QStringLiteral("-") : nv.topNetworkProcess.toHtmlEscaped(),
                nv.topRemoteHost.isEmpty() ? QString() : QStringLiteral("-> %1").arg(nv.topRemoteHost.toHtmlEscaped()))
         + card(QStringLiteral("Network Sessions"), QString::number(nv.totalSessions))
         + card(QStringLiteral("Host Coverage"),
                nv.totalSessions > 0
                    ? QString::number(100 - static_cast<int>(model.dataQuality.unknownHostPct())) + QStringLiteral("%")
                    : QStringLiteral("-"),
                QStringLiteral("attributed hostnames"));
}

// ── Data Quality rows ─────────────────────────────────────────────────────────

QString HtmlDashboardGenerator::buildDataQualityRows(const DataQualitySummary &dq) const
{
    auto row = [](const QString &label, const QString &value, const QString &cls = {}) -> QString {
        return QStringLiteral(
            "<div class='quality-row'>"
            "<span class='quality-label'>%1</span>"
            "<span class='quality-val %2'>%3</span>"
            "</div>"
        ).arg(label, cls, value);
    };

    auto pctClass = [](double pct) -> QString {
        if (pct < 10) return QStringLiteral("good");
        if (pct < 40) return QStringLiteral("warn");
        return QStringLiteral("bad");
    };

    const double uhPct = dq.unknownHostPct();
    const double upPct = dq.unknownProcessPct();

    return row(QStringLiteral("Total network sessions"),         QString::number(dq.totalNetworkSessions))
         + row(QStringLiteral("Sessions with unknown hostname"), QString::number(dq.unknownHostSessions)
               + QStringLiteral(" (") + QString::number(uhPct, 'f', 1) + QStringLiteral("%)"),
               pctClass(uhPct))
         + row(QStringLiteral("Sessions with unknown process"),  QString::number(dq.unknownProcessSessions)
               + QStringLiteral(" (") + QString::number(upPct, 'f', 1) + QStringLiteral("%)"),
               pctClass(upPct))
         + row(QStringLiteral("DNS parser warnings"),           QString::number(dq.dnsWarningCount),
               dq.dnsWarningCount > 0 ? QStringLiteral("warn") : QStringLiteral("good"))
         + row(QStringLiteral("Unrecognised DNS event shapes"), QString::number(dq.unrecognizedDnsEventCount),
               dq.unrecognizedDnsEventCount > 0 ? QStringLiteral("warn") : QStringLiteral("good"))
         + row(QStringLiteral("Shutdown-flushed sessions"),     QString::number(dq.shutdownFlushedSessions))
         + row(QStringLiteral("Low-confidence host sessions"),  QString::number(dq.lowConfidenceSessions));
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString HtmlDashboardGenerator::fmtBytes(quint64 b)
{
    if (b < 1024)       return QString::number(b) + QStringLiteral(" B");
    if (b < 1048576)    return QString::number(b / 1024.0, 'f', 1) + QStringLiteral(" KB");
    if (b < 1073741824) return QString::number(b / 1048576.0, 'f', 1) + QStringLiteral(" MB");
    return QString::number(b / 1073741824.0, 'f', 2) + QStringLiteral(" GB");
}

QString HtmlDashboardGenerator::fmtSecs(qint64 s)
{
    if (s < 60)   return QString::number(s) + QStringLiteral("s");
    if (s < 3600) return QString::number(s / 60) + QStringLiteral("m ") + QString::number(s % 60) + QStringLiteral("s");
    return QString::number(s / 3600) + QStringLiteral("h ") + QString::number((s % 3600) / 60) + QStringLiteral("m");
}

} // namespace Analytics
