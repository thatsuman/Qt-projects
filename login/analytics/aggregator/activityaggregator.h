#ifndef ANALYTICS_AGGREGATOR_ACTIVITYAGGREGATOR_H
#define ANALYTICS_AGGREGATOR_ACTIVITYAGGREGATOR_H

#include "analytics/model/analyticsmodels.h"
#include "analytics/model/activitysummary.h"

#include <QList>

namespace Analytics {

class ActivityAggregator
{
public:
    ActivityAggregator() = default;

    // Aggregate a flat list of activity records into per-process summaries
    // and an overall session overview.
    QList<ActivitySummary> aggregateByProcess(const QList<ActivityRecord> &records);
    ActivityOverview       buildOverview(const QList<ActivityRecord> &records,
                                        const QList<ActivitySummary> &summaries);
};

} // namespace Analytics

#endif // ANALYTICS_AGGREGATOR_ACTIVITYAGGREGATOR_H
