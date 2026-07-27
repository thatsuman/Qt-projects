#ifndef ANALYTICS_READER_ACTIVITYLOGREADER_H
#define ANALYTICS_READER_ACTIVITYLOGREADER_H

#include "analytics/reader/ianalyticreaders.h"

namespace Analytics {

// Reads activity_log.jsonl for a given username.
// Raw keystroke strings are immediately discarded — only the character count is kept.
class ActivityLogReader : public IActivityLogReader
{
public:
    ActivityLogReader() = default;

    QList<ActivityRecord> readLogs(const QString &username) override;
};

} // namespace Analytics

#endif // ANALYTICS_READER_ACTIVITYLOGREADER_H
