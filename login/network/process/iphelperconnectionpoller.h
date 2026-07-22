#ifndef NETWORK_PROCESS_IPHELPERCONNECTIONPOLLER_H
#define NETWORK_PROCESS_IPHELPERCONNECTIONPOLLER_H

#include "network/model/networkevents.h"
#include "network/process/processresolver.h"

#include <QObject>
#include <QTimer>

namespace Network {

class IpHelperConnectionPoller : public QObject
{
    Q_OBJECT

public:
    explicit IpHelperConnectionPoller(QObject *parent = nullptr);

signals:
    void processSnapshotObserved(const ProcessConnectionSnapshot &snapshot);
    void errorOccurred(const QString &message);

public slots:
    void start();
    void stop();

private slots:
    void poll();

private:
    void pollTcp();
    void pollUdp();

    QTimer *m_timer = nullptr;
    ProcessResolver m_processResolver;
};

} // namespace Network

#endif // NETWORK_PROCESS_IPHELPERCONNECTIONPOLLER_H
