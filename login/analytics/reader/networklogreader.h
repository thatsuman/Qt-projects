#ifndef ANALYTICS_READER_NETWORKLOGREADER_H
#define ANALYTICS_READER_NETWORKLOGREADER_H

#include "analytics/reader/ianalyticreaders.h"

namespace Analytics {

// Reads network_log.jsonl for a given username.
class NetworkLogReader : public INetworkLogReader
{
public:
    NetworkLogReader() = default;

    QList<NetworkRecord> readLogs(const QString &username) override;
};

} // namespace Analytics

#endif // ANALYTICS_READER_NETWORKLOGREADER_H
