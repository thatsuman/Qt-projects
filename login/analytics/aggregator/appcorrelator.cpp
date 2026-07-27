#include "appcorrelator.h"

#include <QHash>
#include <algorithm>

namespace Analytics {

QList<AppSummary> AppCorrelator::correlate(const QList<ActivitySummary>       &activityByProcess,
                                            const QList<NetworkProcessSummary> &networkByProcess)
{
    QHash<QString, AppSummary> combined;

    // Seed from activity summaries
    for (const ActivitySummary &act : activityByProcess) {
        const QString key = act.processName.toLower();
        AppSummary &s = combined[key];
        s.processName            = act.processName;
        s.activeDurationSeconds  = act.totalActiveDurationSeconds;
        s.activitySessionCount   = act.sessionCount;
        s.keystrokeCount         = act.totalKeystrokes;
        s.mouseDistancePx        = act.totalMouseDistancePx;
        s.hasActivityData        = true;
    }

    // Merge network summaries (case-insensitive process name match)
    for (const NetworkProcessSummary &net : networkByProcess) {
        const QString key = net.processName.toLower();
        AppSummary &s = combined[key];
        if (s.processName.isEmpty()) s.processName = net.processName;
        s.bytesSent         += net.bytesSent;
        s.bytesReceived     += net.bytesReceived;
        s.totalBytes        += net.totalBytes;
        s.networkSessionCount += net.sessionCount;
        for (const QString &host : net.remoteHosts) {
            if (!s.remoteHosts.contains(host)) s.remoteHosts.append(host);
        }
        s.hasNetworkData = true;
    }

    QList<AppSummary> result = combined.values();
    // Sort by total bytes descending, then by active time descending
    std::sort(result.begin(), result.end(), [](const AppSummary &a, const AppSummary &b) {
        if (a.totalBytes != b.totalBytes) return a.totalBytes > b.totalBytes;
        return a.activeDurationSeconds > b.activeDurationSeconds;
    });
    return result;
}

} // namespace Analytics
