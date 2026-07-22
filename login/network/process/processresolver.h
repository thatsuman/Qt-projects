#ifndef NETWORK_PROCESS_PROCESSRESOLVER_H
#define NETWORK_PROCESS_PROCESSRESOLVER_H

#include <QDateTime>
#include <QHash>
#include <QString>

namespace Network {

struct ProcessInfo
{
    quint32 pid = 0;
    QString name = "unknown";
    QString path;
    QDateTime creationTimeUtc;
};

class ProcessResolver
{
public:
    ProcessInfo resolve(quint32 pid);

private:
    struct CacheEntry
    {
        ProcessInfo info;
        QDateTime cachedUtc;
    };

    QHash<quint32, CacheEntry> m_cache;
    int m_cacheTtlSeconds = 10;
};

} // namespace Network

#endif // NETWORK_PROCESS_PROCESSRESOLVER_H
