#ifndef NETWORK_DNS_DNSCACHE_H
#define NETWORK_DNS_DNSCACHE_H

#include "network/model/networkevents.h"
#include "network/model/flowsession.h"

#include <QHash>

namespace Network {

class DnsCache
{
public:
    explicit DnsCache(int staleWindowSeconds = 3600);

    void addObservation(const DnsObservation &observation);
    HostnameAttribution lookup(const IpAddress &remoteIp, quint32 pid, const QDateTime &flowStartTimeUtc);
    void evictExpired(const QDateTime &nowUtc);

private:
    struct Candidate
    {
        QString hostname;
        quint32 pid = 0;
        QDateTime observedUtc;
        QDateTime expiresUtc;
        QString source;
    };

    int m_staleWindowSeconds = 3600;
    QHash<IpAddress, QList<Candidate>> m_candidatesByIp;
};

} // namespace Network

#endif // NETWORK_DNS_DNSCACHE_H
