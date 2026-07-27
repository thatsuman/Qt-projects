#include "timelineaggregator.h"

#include <QHash>
#include <algorithm>
#include <cmath>

namespace Analytics {

TimelineAggregator::TimelineAggregator(int intervalSeconds)
    : m_intervalSeconds(intervalSeconds > 0 ? intervalSeconds : 60)
{
}

QList<TimelineBucket> TimelineAggregator::aggregate(const QList<ActivityRecord> &activityRecords,
                                                     const QList<NetworkRecord>  &networkRecords)
{
    if (activityRecords.isEmpty() && networkRecords.isEmpty()) return {};

    // Determine time span
    QDateTime earliest, latest;

    for (const ActivityRecord &rec : activityRecords) {
        if (rec.startTime.isValid()) {
            if (!earliest.isValid() || rec.startTime < earliest) earliest = rec.startTime;
        }
        if (rec.endTime.isValid()) {
            if (!latest.isValid() || rec.endTime > latest) latest = rec.endTime;
        }
    }
    for (const NetworkRecord &rec : networkRecords) {
        if (rec.startTime.isValid()) {
            if (!earliest.isValid() || rec.startTime < earliest) earliest = rec.startTime;
        }
        if (rec.endTime.isValid()) {
            if (!latest.isValid() || rec.endTime > latest) latest = rec.endTime;
        }
    }

    if (!earliest.isValid() || !latest.isValid()) return {};

    // Snap earliest back to a clean bucket boundary
    const qint64 epochSecs    = earliest.toSecsSinceEpoch();
    const qint64 bucketOrigin = (epochSecs / m_intervalSeconds) * m_intervalSeconds;

    const qint64 totalSpan    = earliest.secsTo(latest);
    const int    bucketCount  = static_cast<int>(std::ceil(static_cast<double>(totalSpan) / m_intervalSeconds)) + 1;

    QList<TimelineBucket> buckets;
    buckets.reserve(bucketCount);
    for (int i = 0; i < bucketCount; ++i) {
        TimelineBucket b;
        b.bucketStart     = QDateTime::fromSecsSinceEpoch(bucketOrigin + static_cast<qint64>(i) * m_intervalSeconds).toUTC();
        b.bucketEnd       = QDateTime::fromSecsSinceEpoch(bucketOrigin + static_cast<qint64>(i + 1) * m_intervalSeconds).toUTC();
        b.intervalSeconds = m_intervalSeconds;
        buckets.append(b);
    }

    // Helper lambda: return bucket index for a given UTC time
    auto bucketIndex = [&](const QDateTime &dt) -> int {
        if (!dt.isValid()) return -1;
        const qint64 secs = dt.toSecsSinceEpoch() - bucketOrigin;
        const int idx = static_cast<int>(secs / m_intervalSeconds);
        return (idx >= 0 && idx < buckets.size()) ? idx : -1;
    };

    // Distribute activity records into buckets
    for (const ActivityRecord &rec : activityRecords) {
        if (!rec.startTime.isValid()) continue;
        QDateTime cur = rec.startTime;
        const QDateTime end = rec.endTime.isValid() ? rec.endTime : cur.addSecs(2);
        while (cur < end) {
            const int idx = bucketIndex(cur);
            if (idx >= 0) {
                TimelineBucket &b = buckets[idx];
                const QDateTime bucketEnd = b.bucketEnd;
                const qint64 overlap = cur.secsTo(qMin(end, bucketEnd));
                if (overlap > 0) b.activeSeconds += overlap;
                // Track which process dominated this bucket by activity duration
                b.foregroundProcess = rec.processName;
            }
            cur = cur.addSecs(m_intervalSeconds);
        }
    }

    // Distribute network records into buckets
    for (const NetworkRecord &rec : networkRecords) {
        if (!rec.startTime.isValid()) continue;
        // Attribute traffic to the bucket where the session started
        const int idx = bucketIndex(rec.startTime);
        if (idx >= 0) {
            TimelineBucket &b = buckets[idx];
            b.bytesSent      += rec.bytesSent;
            b.bytesReceived  += rec.bytesReceived;
            b.totalBytes     += rec.totalBytes();
            b.networkSessions++;
        }
    }

    return buckets;
}

} // namespace Analytics
