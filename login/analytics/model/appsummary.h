#ifndef ANALYTICS_MODEL_APPSUMMARY_H
#define ANALYTICS_MODEL_APPSUMMARY_H

#include <QString>
#include <QStringList>

namespace Analytics {

// Combined activity + network metrics per application (by process name).
// Correlation is approximate — based on process name match and time overlap.
struct AppSummary
{
    QString processName;

    // Activity-derived metrics
    qint64  activeDurationSeconds  = 0;
    int     activitySessionCount   = 0;
    int     keystrokeCount         = 0;
    double  mouseDistancePx        = 0.0;
    bool    hasActivityData        = false;

    // Network-derived metrics
    quint64 bytesSent              = 0;
    quint64 bytesReceived          = 0;
    quint64 totalBytes             = 0;
    int     networkSessionCount    = 0;
    QStringList remoteHosts;
    bool    hasNetworkData         = false;

    // Correlation metadata
    // Always "Approximate" — process name + time window matching, not causal proof
    QString correlationConfidence  = QStringLiteral("Approximate");
};

} // namespace Analytics

#endif // ANALYTICS_MODEL_APPSUMMARY_H
