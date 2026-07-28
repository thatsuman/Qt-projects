#ifndef ANALYTICS_MODEL_ACTIVITYSUMMARY_H
#define ANALYTICS_MODEL_ACTIVITYSUMMARY_H

#include <QDateTime>
#include <QString>

namespace Analytics {

// Per-process aggregated activity metrics for one session.
struct ActivitySummary
{
    QString processName;
    qint64  totalActiveDurationSeconds = 0;
    int     sessionCount               = 0;
    int     totalKeystrokes            = 0;
    double  totalMouseDistancePx       = 0.0;
    QDateTime firstSeen;
    QDateTime lastSeen;
};

// Session-level overview metrics (across all processes).
struct ActivityOverview
{
    qint64    totalSessionSeconds      = 0;  // wall-clock from first to last record
    qint64    totalActiveSeconds       = 0;  // sum of all activity record durations
    int       totalKeystrokes          = 0;
    double    totalMouseDistancePx     = 0.0;
    QString   topActiveProcess;              // process with most active time
    QDateTime sessionStart;
    QDateTime sessionEnd;
    int       totalAppSessions         = 0;  // total number of activity log records
};

} // namespace Analytics

#endif // ANALYTICS_MODEL_ACTIVITYSUMMARY_H
