#ifndef ANALYTICS_AGGREGATOR_TIMELINEAGGREGATOR_H
#define ANALYTICS_AGGREGATOR_TIMELINEAGGREGATOR_H

#include "analytics/model/analyticsmodels.h"
#include "analytics/model/timelinebucket.h"

#include <QList>

namespace Analytics {

class TimelineAggregator
{
public:
    explicit TimelineAggregator(int intervalSeconds = 60);

    QList<TimelineBucket> aggregate(const QList<ActivityRecord> &activityRecords,
                                    const QList<NetworkRecord>  &networkRecords);

private:
    int m_intervalSeconds;
};

} // namespace Analytics

#endif // ANALYTICS_AGGREGATOR_TIMELINEAGGREGATOR_H
