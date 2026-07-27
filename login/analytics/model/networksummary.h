#ifndef ANALYTICS_MODEL_NETWORKSUMMARY_H
#define ANALYTICS_MODEL_NETWORKSUMMARY_H

#include <QString>
#include <QStringList>
#include <QHash>

namespace Analytics {

// Per-process network traffic totals.
struct NetworkProcessSummary
{
    QString processName;
    quint64 bytesSent         = 0;
    quint64 bytesReceived     = 0;
    quint64 totalBytes        = 0;
    quint64 packetsSent       = 0;
    quint64 packetsReceived   = 0;
    int     sessionCount      = 0;
    QStringList remoteHosts;    // unique remote hostnames/IPs contacted
    QStringList protocolHints;  // unique protocol hints observed
};

// Per remote host (hostname or IP) traffic totals.
struct NetworkRemoteSummary
{
    QString displayHost;        // hostname if attributed, else IP
    QString remoteIp;
    QString hostnameConfidence;
    quint64 totalBytesSent     = 0;
    quint64 totalBytesReceived = 0;
    int     sessionCount       = 0;
    QStringList protocols;
    QStringList processes;      // which processes connected to this host
};

// Session-level network overview.
struct NetworkOverview
{
    quint64 totalBytesSent     = 0;
    quint64 totalBytesReceived = 0;
    quint64 totalBytes         = 0;
    int     totalSessions      = 0;
    int     unknownHostCount   = 0;
    int     unknownProcessCount = 0;
    QString topNetworkProcess;   // by total bytes
    QString topRemoteHost;       // by total bytes
    int     hostnameAttributedCount = 0;
};

} // namespace Analytics

#endif // ANALYTICS_MODEL_NETWORKSUMMARY_H
