#ifndef ANALYTICS_AGGREGATOR_APPCORRELATOR_H
#define ANALYTICS_AGGREGATOR_APPCORRELATOR_H

#include "analytics/model/analyticsmodels.h"
#include "analytics/model/activitysummary.h"
#include "analytics/model/networksummary.h"
#include "analytics/model/appsummary.h"

#include <QList>

namespace Analytics {

// Correlates ActivitySummary and NetworkProcessSummary by process name.
// Correlation is always labeled "Approximate" — not a causal guarantee.
class AppCorrelator
{
public:
    AppCorrelator() = default;

    QList<AppSummary> correlate(const QList<ActivitySummary>        &activityByProcess,
                                const QList<NetworkProcessSummary>  &networkByProcess);
};

} // namespace Analytics

#endif // ANALYTICS_AGGREGATOR_APPCORRELATOR_H
