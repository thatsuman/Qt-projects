#include "activityaggregator.h"

#include <QHash>
#include <algorithm>

namespace Analytics {

QList<ActivitySummary> ActivityAggregator::aggregateByProcess(const QList<ActivityRecord> &records)
{
    QHash<QString, ActivitySummary> byProcess;

    for (const ActivityRecord &rec : records) {
        const QString key = rec.processName.toLower();
        if (!byProcess.contains(key)) {
            ActivitySummary s;
            s.processName = rec.processName;
            byProcess.insert(key, s);
        }

        ActivitySummary &s = byProcess[key];
        s.sessionCount++;
        s.totalKeystrokes     += rec.keystrokeCount;
        s.totalMouseDistancePx += rec.mouseDistancePx;

        const qint64 dur = rec.durationSeconds();
        if (dur > 0) s.totalActiveDurationSeconds += dur;

        if (rec.startTime.isValid()) {
            if (!s.firstSeen.isValid() || rec.startTime < s.firstSeen) s.firstSeen = rec.startTime;
        }
        if (rec.endTime.isValid()) {
            if (!s.lastSeen.isValid() || rec.endTime > s.lastSeen) s.lastSeen = rec.endTime;
        }
    }

    QList<ActivitySummary> result = byProcess.values();
    // Sort descending by active time
    std::sort(result.begin(), result.end(), [](const ActivitySummary &a, const ActivitySummary &b) {
        return a.totalActiveDurationSeconds > b.totalActiveDurationSeconds;
    });
    return result;
}

ActivityOverview ActivityAggregator::buildOverview(const QList<ActivityRecord> &records,
                                                    const QList<ActivitySummary> &summaries)
{
    ActivityOverview ov;
    ov.totalAppSessions = records.size();

    for (const ActivityRecord &rec : records) {
        ov.totalKeystrokes      += rec.keystrokeCount;
        ov.totalMouseDistancePx += rec.mouseDistancePx;
        const qint64 dur = rec.durationSeconds();
        if (dur > 0) ov.totalActiveSeconds += dur;

        if (rec.startTime.isValid()) {
            if (!ov.sessionStart.isValid() || rec.startTime < ov.sessionStart)
                ov.sessionStart = rec.startTime;
        }
        if (rec.endTime.isValid()) {
            if (!ov.sessionEnd.isValid() || rec.endTime > ov.sessionEnd)
                ov.sessionEnd = rec.endTime;
        }
    }

    if (ov.sessionStart.isValid() && ov.sessionEnd.isValid()) {
        ov.totalSessionSeconds = ov.sessionStart.secsTo(ov.sessionEnd);
    }

    if (!summaries.isEmpty()) {
        ov.topActiveProcess = summaries.first().processName; // already sorted descending
    }

    return ov;
}

} // namespace Analytics
