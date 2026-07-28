#ifndef ANALYTICS_AGGREGATOR_NETWORKAGGREGATOR_H
#define ANALYTICS_AGGREGATOR_NETWORKAGGREGATOR_H

#include "analytics/model/analyticsmodels.h"
#include "analytics/model/networksummary.h"

#include <QList>

namespace Analytics {

class NetworkAggregator
{
public:
    NetworkAggregator() = default;

    QList<NetworkProcessSummary> aggregateByProcess(const QList<NetworkRecord> &records);
    QList<NetworkRemoteSummary>  aggregateByRemoteHost(const QList<NetworkRecord> &records);
    NetworkOverview              buildOverview(const QList<NetworkRecord> &records,
                                              const QList<NetworkProcessSummary> &byProcess,
                                              const QList<NetworkRemoteSummary>  &byRemote);
};

} // namespace Analytics

#endif // ANALYTICS_AGGREGATOR_NETWORKAGGREGATOR_H
