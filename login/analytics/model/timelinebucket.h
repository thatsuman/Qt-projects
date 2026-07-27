#ifndef ANALYTICS_MODEL_TIMELINEBUCKET_H
#define ANALYTICS_MODEL_TIMELINEBUCKET_H

#include <QDateTime>
#include <QString>

namespace Analytics {

// One time bucket (default 60 seconds) for timeline aggregation.
struct TimelineBucket
{
    QDateTime bucketStart;          // UTC start of this interval
    QDateTime bucketEnd;            // UTC end of this interval (bucketStart + interval)
    int       intervalSeconds = 60;

    // Activity in this bucket
    QString   foregroundProcess;    // the process with the most foreground time in bucket
    qint64    activeSeconds    = 0; // total foreground seconds within bucket

    // Network traffic in this bucket
    quint64   bytesSent        = 0;
    quint64   bytesReceived    = 0;
    quint64   totalBytes       = 0;
    int       networkSessions  = 0;
};

} // namespace Analytics

#endif // ANALYTICS_MODEL_TIMELINEBUCKET_H
