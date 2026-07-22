#ifndef NETWORK_ORCHESTRATOR_NETWORKORCHESTRATOR_H
#define NETWORK_ORCHESTRATOR_NETWORKORCHESTRATOR_H

#include <QObject>
#include <QList>

class QThread;

namespace Network {

class EtwDnsMonitor;
class FlowManager;
class IpHelperConnectionPoller;
class NetworkJsonlWriter;
class WinDivertFlowCapture;
class WinDivertPacketCapture;

class NetworkOrchestrator : public QObject
{
    Q_OBJECT

public:
    explicit NetworkOrchestrator(QObject *parent = nullptr);
    ~NetworkOrchestrator() override;

    bool isActive() const;

signals:
    void statusChanged(const QString &status);

public slots:
    void start(const QString &username);
    void stop();

private:
    template <typename T>
    T *createWorker(QThread *thread);

    void stopThread(QThread *thread);
    void registerMetaTypes();

    bool m_active = false;
    QString m_username;

    QThread *m_packetThread = nullptr;
    QThread *m_flowCaptureThread = nullptr;
    QThread *m_flowManagerThread = nullptr;
    QThread *m_dnsThread = nullptr;
    QThread *m_ipHelperThread = nullptr;
    QThread *m_writerThread = nullptr;

    WinDivertPacketCapture *m_packetCapture = nullptr;
    WinDivertFlowCapture *m_flowCapture = nullptr;
    FlowManager *m_flowManager = nullptr;
    EtwDnsMonitor *m_dnsMonitor = nullptr;
    IpHelperConnectionPoller *m_ipHelperPoller = nullptr;
    NetworkJsonlWriter *m_writer = nullptr;
};

} // namespace Network

#endif // NETWORK_ORCHESTRATOR_NETWORKORCHESTRATOR_H
