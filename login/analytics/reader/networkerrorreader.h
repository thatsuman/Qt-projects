#ifndef ANALYTICS_READER_NETWORKERRORREADER_H
#define ANALYTICS_READER_NETWORKERRORREADER_H

#include "analytics/model/analyticsmodels.h"

#include <QList>
#include <QString>

namespace Analytics {

// Reads network_error.txt and network_dns_diag.jsonl for a given username.
class NetworkErrorReader
{
public:
    NetworkErrorReader() = default;

    QList<ErrorRecord> readErrorLog(const QString &username);
    QList<DiagRecord>  readDiagLog(const QString &username);
};

} // namespace Analytics

#endif // ANALYTICS_READER_NETWORKERRORREADER_H
