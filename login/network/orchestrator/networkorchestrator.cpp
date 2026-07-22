#include "networkorchestrator.h"

#include "network/capture/windivertflowcapture.h"
#include "network/capture/windivertpacketcapture.h"
#include "network/dns/etwdnsmonitor.h"
#include "network/flow/flowmanager.h"
#include "network/merge/consecutiverecordmerger.h"
#include "network/model/networkevents.h"
#include "network/model/networksessionrecord.h"
#include "network/process/iphelperconnectionpoller.h"
#include "network/writer/networkjsonlwriter.h"

#include <QMetaObject>
#include <QThread>

namespace Network {

NetworkOrchestrator::NetworkOrchestrator(QObject *parent)
    : QObject(parent)
{
    registerMetaTypes();
}

NetworkOrchestrator::~NetworkOrchestrator()
{
    stop();
}

bool NetworkOrchestrator::isActive() const
{
    return m_active;
}

void NetworkOrchestrator::start(const QString &username)
{
    if (m_active) {
        return;
    }

    m_username = username;

    m_packetThread = new QThread(this);
    m_flowCaptureThread = new QThread(this);
    m_flowManagerThread = new QThread(this);
    m_dnsThread = new QThread(this);
    m_ipHelperThread = new QThread(this);
    m_writerThread = new QThread(this);

    m_packetCapture = createWorker<WinDivertPacketCapture>(m_packetThread);
    m_flowCapture = createWorker<WinDivertFlowCapture>(m_flowCaptureThread);
    m_flowManager = createWorker<FlowManager>(m_flowManagerThread);
    m_dnsMonitor = createWorker<EtwDnsMonitor>(m_dnsThread);
    m_ipHelperPoller = createWorker<IpHelperConnectionPoller>(m_ipHelperThread);
    m_merger = createWorker<ConsecutiveRecordMerger>(m_writerThread);
    m_writer = createWorker<NetworkJsonlWriter>(m_writerThread);

    connect(m_packetCapture, &WinDivertPacketCapture::packetObserved,
            m_flowManager, &FlowManager::handlePacket, Qt::QueuedConnection);
    connect(m_flowCapture, &WinDivertFlowCapture::flowLifecycleObserved,
            m_flowManager, &FlowManager::handleFlowLifecycle, Qt::QueuedConnection);
    connect(m_ipHelperPoller, &IpHelperConnectionPoller::processSnapshotObserved,
            m_flowManager, &FlowManager::handleProcessSnapshot, Qt::QueuedConnection);
    connect(m_dnsMonitor, &EtwDnsMonitor::dnsObserved,
            m_flowManager, &FlowManager::handleDnsObservation, Qt::QueuedConnection);
    connect(m_flowManager, &FlowManager::sessionClosed,
            m_merger, &ConsecutiveRecordMerger::processSessionRecord, Qt::QueuedConnection);
    connect(m_merger, &ConsecutiveRecordMerger::recordReadyForLogging,
            m_writer, &NetworkJsonlWriter::writeSession, Qt::QueuedConnection);

    const auto connectError = [this](QObject *source) {
        connect(source, SIGNAL(errorOccurred(QString)),
                m_writer, SLOT(logError(QString)), Qt::QueuedConnection);
        connect(source, SIGNAL(errorOccurred(QString)),
                this, SIGNAL(statusChanged(QString)), Qt::QueuedConnection);
    };
    connectError(m_packetCapture);
    connectError(m_flowCapture);
    connectError(m_flowManager);
    connectError(m_dnsMonitor);
    connectError(m_ipHelperPoller);

    connect(m_packetCapture, &WinDivertPacketCapture::statusChanged, this, &NetworkOrchestrator::statusChanged);
    connect(m_flowCapture, &WinDivertFlowCapture::statusChanged, this, &NetworkOrchestrator::statusChanged);
    connect(m_dnsMonitor, &EtwDnsMonitor::statusChanged, this, &NetworkOrchestrator::statusChanged);

    m_writerThread->start();
    m_flowManagerThread->start();
    m_dnsThread->start();
    m_ipHelperThread->start();
    m_packetThread->start();
    m_flowCaptureThread->start();

    QMetaObject::invokeMethod(m_writer, "start", Qt::BlockingQueuedConnection, Q_ARG(QString, username));
    QMetaObject::invokeMethod(m_flowManager, "start", Qt::BlockingQueuedConnection, Q_ARG(QString, username));
    QMetaObject::invokeMethod(m_dnsMonitor, "start", Qt::QueuedConnection);
    QMetaObject::invokeMethod(m_ipHelperPoller, "start", Qt::QueuedConnection);
    QMetaObject::invokeMethod(m_packetCapture, "start", Qt::QueuedConnection);
    QMetaObject::invokeMethod(m_flowCapture, "start", Qt::QueuedConnection);

    m_active = true;
    emit statusChanged("Network logging started");
}

void NetworkOrchestrator::stop()
{
    if (!m_active) {
        return;
    }

    if (m_packetCapture) {
        m_packetCapture->stop();
    }
    if (m_flowCapture) {
        m_flowCapture->stop();
    }
    if (m_dnsMonitor) {
        m_dnsMonitor->stop();
    }
    if (m_ipHelperPoller) {
        QMetaObject::invokeMethod(m_ipHelperPoller, "stop", Qt::BlockingQueuedConnection);
    }
    if (m_flowManager) {
        QMetaObject::invokeMethod(m_flowManager, "flushAll", Qt::BlockingQueuedConnection,
                                  Q_ARG(QString, QString("app_shutdown")));
        QMetaObject::invokeMethod(m_flowManager, "stop", Qt::BlockingQueuedConnection);
    }
    if (m_merger) {
        QMetaObject::invokeMethod(m_merger, "flush", Qt::BlockingQueuedConnection);
    }
    if (m_writer) {
        QMetaObject::invokeMethod(m_writer, "stop", Qt::BlockingQueuedConnection);
    }

    stopThread(m_packetThread);
    stopThread(m_flowCaptureThread);
    stopThread(m_dnsThread);
    stopThread(m_ipHelperThread);
    stopThread(m_flowManagerThread);
    stopThread(m_writerThread);

    m_packetThread = nullptr;
    m_flowCaptureThread = nullptr;
    m_flowManagerThread = nullptr;
    m_dnsThread = nullptr;
    m_ipHelperThread = nullptr;
    m_writerThread = nullptr;
    m_packetCapture = nullptr;
    m_flowCapture = nullptr;
    m_flowManager = nullptr;
    m_dnsMonitor = nullptr;
    m_ipHelperPoller = nullptr;
    m_merger = nullptr;
    m_writer = nullptr;
    m_active = false;
    m_username.clear();

    emit statusChanged("Network logging stopped");
}

template <typename T>
T *NetworkOrchestrator::createWorker(QThread *thread)
{
    T *worker = new T();
    worker->moveToThread(thread);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    return worker;
}

void NetworkOrchestrator::stopThread(QThread *thread)
{
    if (!thread) {
        return;
    }

    thread->quit();
    thread->wait(5000);
    thread->deleteLater();
}

void NetworkOrchestrator::registerMetaTypes()
{
    qRegisterMetaType<PacketObservation>("Network::PacketObservation");
    qRegisterMetaType<FlowLifecycleObservation>("Network::FlowLifecycleObservation");
    qRegisterMetaType<ProcessConnectionSnapshot>("Network::ProcessConnectionSnapshot");
    qRegisterMetaType<DnsObservation>("Network::DnsObservation");
    qRegisterMetaType<NetworkSessionRecord>("Network::NetworkSessionRecord");
    qRegisterMetaType<PacketObservation>("PacketObservation");
    qRegisterMetaType<FlowLifecycleObservation>("FlowLifecycleObservation");
    qRegisterMetaType<ProcessConnectionSnapshot>("ProcessConnectionSnapshot");
    qRegisterMetaType<DnsObservation>("DnsObservation");
    qRegisterMetaType<NetworkSessionRecord>("NetworkSessionRecord");
}

} // namespace Network
