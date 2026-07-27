#include "networkaggregator.h"

#include <QHash>
#include <algorithm>

namespace Analytics {

QList<NetworkProcessSummary> NetworkAggregator::aggregateByProcess(const QList<NetworkRecord> &records)
{
    QHash<QString, NetworkProcessSummary> byProc;

    for (const NetworkRecord &rec : records) {
        const QString key = rec.processName.isEmpty() ? QStringLiteral("unknown") : rec.processName.toLower();
        if (!byProc.contains(key)) {
            NetworkProcessSummary s;
            s.processName = rec.processName.isEmpty() ? QStringLiteral("unknown") : rec.processName;
            byProc.insert(key, s);
        }

        NetworkProcessSummary &s = byProc[key];
        s.bytesSent      += rec.bytesSent;
        s.bytesReceived  += rec.bytesReceived;
        s.totalBytes     += rec.totalBytes();
        s.packetsSent    += rec.packetsSent;
        s.packetsReceived += rec.packetsReceived;
        s.sessionCount++;

        const QString host = rec.displayHost();
        if (!host.isEmpty() && !s.remoteHosts.contains(host)) s.remoteHosts.append(host);
        if (!rec.appProtocolHint.isEmpty() && !s.protocolHints.contains(rec.appProtocolHint))
            s.protocolHints.append(rec.appProtocolHint);
    }

    QList<NetworkProcessSummary> result = byProc.values();
    std::sort(result.begin(), result.end(), [](const NetworkProcessSummary &a, const NetworkProcessSummary &b) {
        return a.totalBytes > b.totalBytes;
    });
    return result;
}

QList<NetworkRemoteSummary> NetworkAggregator::aggregateByRemoteHost(const QList<NetworkRecord> &records)
{
    QHash<QString, NetworkRemoteSummary> byHost;

    for (const NetworkRecord &rec : records) {
        const QString displayHost = rec.displayHost();
        const QString key = displayHost.toLower();
        if (!byHost.contains(key)) {
            NetworkRemoteSummary s;
            s.displayHost        = displayHost;
            s.remoteIp           = rec.remoteIp;
            s.hostnameConfidence = rec.hostnameConfidence;
            byHost.insert(key, s);
        }

        NetworkRemoteSummary &s = byHost[key];
        s.totalBytesSent     += rec.bytesSent;
        s.totalBytesReceived += rec.bytesReceived;
        s.sessionCount++;
        if (!rec.appProtocolHint.isEmpty() && !s.protocols.contains(rec.appProtocolHint))
            s.protocols.append(rec.appProtocolHint);
        if (!rec.processName.isEmpty() && !s.processes.contains(rec.processName))
            s.processes.append(rec.processName);
    }

    QList<NetworkRemoteSummary> result = byHost.values();
    std::sort(result.begin(), result.end(), [](const NetworkRemoteSummary &a, const NetworkRemoteSummary &b) {
        return (a.totalBytesSent + a.totalBytesReceived) > (b.totalBytesSent + b.totalBytesReceived);
    });
    return result;
}

NetworkOverview NetworkAggregator::buildOverview(const QList<NetworkRecord> &records,
                                                  const QList<NetworkProcessSummary> &byProcess,
                                                  const QList<NetworkRemoteSummary>  &byRemote)
{
    NetworkOverview ov;
    ov.totalSessions = records.size();

    for (const NetworkRecord &rec : records) {
        ov.totalBytesSent     += rec.bytesSent;
        ov.totalBytesReceived += rec.bytesReceived;
        ov.totalBytes         += rec.totalBytes();
        if (rec.remoteHostname.isEmpty())
            ov.unknownHostCount++;
        else
            ov.hostnameAttributedCount++;

        if (rec.processName.isEmpty() || rec.processName == QStringLiteral("unknown"))
            ov.unknownProcessCount++;
    }

    if (!byProcess.isEmpty()) ov.topNetworkProcess = byProcess.first().processName;
    if (!byRemote.isEmpty())  ov.topRemoteHost     = byRemote.first().displayHost;

    return ov;
}

} // namespace Analytics
