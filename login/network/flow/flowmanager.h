#ifndef NETWORK_FLOW_FLOWMANAGER_H
#define NETWORK_FLOW_FLOWMANAGER_H

#include "network/dns/dnscache.h"
#include "network/model/networkevents.h"
#include "network/model/networksessionrecord.h"

#include <QHash>
#include <QObject>
#include <QTimer>

namespace Network {

class FlowManager : public QObject
{
    Q_OBJECT

public:
    explicit FlowManager(QObject *parent = nullptr);

signals:
    void sessionClosed(const NetworkSessionRecord &record);
    void errorOccurred(const QString &message);

public slots:
    void start(const QString &username);
    void stop();
    void handlePacket(const PacketObservation &packet);
    void handleFlowLifecycle(const FlowLifecycleObservation &observation);
    void handleProcessSnapshot(const ProcessConnectionSnapshot &snapshot);
    void handleDnsObservation(const DnsObservation &observation);
    void flushAll(const QString &closeReason);

private slots:
    void sweepIdleFlows();

private:
    FlowSession &sessionForPacket(const PacketObservation &packet, const FlowKey &key);
    void closeFlow(const FlowKey &key, const QString &closeReason, const QDateTime &endTimeUtc);
    NetworkSessionRecord makeRecord(const FlowSession &session, const QString &closeReason, const QDateTime &endTimeUtc);
    void applyProcess(FlowSession &session, quint32 pid, const QString &name, const QString &path,
                      const QString &source, const QString &confidence);

    QString m_username;
    QHash<FlowKey, FlowSession> m_sessions;
    DnsCache m_dnsCache;
    QTimer *m_sweepTimer = nullptr;
    int m_tcpIdleSeconds = 120;
    int m_udpIdleSeconds = 30;
    int m_tcpFinGraceSeconds = 5;
};

} // namespace Network

#endif // NETWORK_FLOW_FLOWMANAGER_H
