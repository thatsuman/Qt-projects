#ifndef ANALYTICS_READER_IANALYTICREADERS_H
#define ANALYTICS_READER_IANALYTICREADERS_H

#include "analytics/model/analyticsmodels.h"

#include <QList>
#include <QString>

namespace Analytics {

// ── IActivityLogReader ────────────────────────────────────────────────────────
// Abstract interface for reading activity records.
// Implementations: ActivityLogReader (JSONL), future SqliteActivityReader.
class IActivityLogReader
{
public:
    virtual ~IActivityLogReader() = default;

    // Read all activity records for the given username.
    // Returns an empty list (not an error) if the log file does not exist.
    virtual QList<ActivityRecord> readLogs(const QString &username) = 0;
};

// ── INetworkLogReader ─────────────────────────────────────────────────────────
// Abstract interface for reading network session records.
// Implementations: NetworkLogReader (JSONL), future SqliteNetworkReader.
class INetworkLogReader
{
public:
    virtual ~INetworkLogReader() = default;

    // Read all network session records for the given username.
    // Returns an empty list (not an error) if the log file does not exist.
    virtual QList<NetworkRecord> readLogs(const QString &username) = 0;
};

} // namespace Analytics

#endif // ANALYTICS_READER_IANALYTICREADERS_H
