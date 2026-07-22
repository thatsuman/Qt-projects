#include "flowmanager.h"

#include "network/protocol/protocolinferencer.h"

namespace Network {

namespace {

int processConfidenceRank(const QString &confidence)
{
    if (confidence == "high") return 3;
    if (confidence == "medium") return 2;
    if (confidence == "low") return 1;
    return 0;
}

} // namespace

FlowManager::FlowManager(QObject *parent)
    : QObject(parent)
    , m_sweepTimer(new QTimer(this))
{
    m_sweepTimer->setInterval(1000);
    connect(m_sweepTimer, &QTimer::timeout, this, &FlowManager::sweepIdleFlows);
}

void FlowManager::start(const QString &username)
{
    m_username = username;
    m_sessions.clear();
    m_sweepTimer->start();
}

void FlowManager::stop()
{
    flushAll("app_shutdown");
    m_sweepTimer->stop();
    m_username.clear();
}

void FlowManager::handlePacket(const PacketObservation &packet)
{
    if (m_username.isEmpty()) {
        return;
    }

    const FlowKey key = normalizePacketKey(packet);
    FlowSession &session = sessionForPacket(packet, key);
    const bool outbound = packet.direction != Direction::Inbound;

    session.lastSeenUtc = packet.timestampUtc.isValid()
            ? packet.timestampUtc.toUTC()
            : QDateTime::currentDateTimeUtc();
    session.loopback = session.loopback || packet.loopback || key.localIp.isLoopback() || key.remoteIp.isLoopback();
    session.ipv6 = key.ipVersion == 6;

    if (outbound) {
        session.bytesSentTotal += packet.packetBytes;
        session.payloadBytesSent += packet.payloadBytes;
        session.packetsSent += 1;
    } else {
        session.bytesReceivedTotal += packet.packetBytes;
        session.payloadBytesReceived += packet.payloadBytes;
        session.packetsReceived += 1;
    }

    if (packet.transport == TransportProtocol::Tcp) {
        session.tcpSynSeen = session.tcpSynSeen || packet.tcpFlags.syn;
        session.tcpFinSeen = session.tcpFinSeen || packet.tcpFlags.fin;
        session.tcpRstSeen = session.tcpRstSeen || packet.tcpFlags.rst;
        if (packet.tcpFlags.fin) {
            session.tcpFinSeenUtc = session.lastSeenUtc;
        }
        if (packet.tcpFlags.rst) {
            closeFlow(key, "tcp_rst", session.lastSeenUtc);
        }
    }
}

void FlowManager::handleFlowLifecycle(const FlowLifecycleObservation &observation)
{
    if (observation.event == FlowLifecycleEvent::Deleted) {
        if (m_sessions.contains(observation.key)) {
            closeFlow(observation.key, "flow_deleted", observation.timestampUtc.toUTC());
        }
        return;
    }

    auto it = m_sessions.find(observation.key);
    if (it != m_sessions.end()) {
        applyProcess(it.value(), observation.pid, QString(), QString(), observation.source, "high");
    }
}

void FlowManager::handleProcessSnapshot(const ProcessConnectionSnapshot &snapshot)
{
    auto it = m_sessions.find(snapshot.key);
    if (it == m_sessions.end()) {
        return;
    }

    if (snapshot.processCreationTimeUtc.isValid()
            && it.value().startTimeUtc.isValid()
            && snapshot.processCreationTimeUtc > it.value().startTimeUtc.addMSecs(1000)) {
        emit errorOccurred(QString("Ignoring process snapshot for PID %1 because creation time is newer than flow start.")
                           .arg(snapshot.pid));
        return;
    }

    applyProcess(it.value(), snapshot.pid, snapshot.processName, snapshot.processPath, snapshot.source, "medium");
}

void FlowManager::handleDnsObservation(const DnsObservation &observation)
{
    m_dnsCache.addObservation(observation);

    if (observation.pid == 0 || observation.answerIps.isEmpty()) {
        return;
    }

    for (FlowSession &session : m_sessions) {
        if (observation.answerIps.contains(session.key.remoteIp)) {
            applyProcess(session, observation.pid, observation.processName, QString(),
                         observation.source, "low");
        }
    }
}

void FlowManager::flushAll(const QString &closeReason)
{
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    const QList<FlowKey> keys = m_sessions.keys();
    for (const FlowKey &key : keys) {
        closeFlow(key, closeReason, nowUtc);
    }
}

void FlowManager::sweepIdleFlows()
{
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    QList<QPair<FlowKey, QString>> toClose;

    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        const FlowSession &session = it.value();
        const qint64 idleSeconds = session.lastSeenUtc.secsTo(nowUtc);

        if (session.key.transport == TransportProtocol::Tcp && session.tcpFinSeen
                && session.tcpFinSeenUtc.secsTo(nowUtc) >= m_tcpFinGraceSeconds) {
            toClose.append(qMakePair(it.key(), QString("tcp_fin")));
        } else if (session.key.transport == TransportProtocol::Tcp && idleSeconds >= m_tcpIdleSeconds) {
            toClose.append(qMakePair(it.key(), QString("idle_timeout")));
        } else if (session.key.transport == TransportProtocol::Udp && idleSeconds >= m_udpIdleSeconds) {
            toClose.append(qMakePair(it.key(), QString("idle_timeout")));
        }
    }

    for (const auto &item : toClose) {
        closeFlow(item.first, item.second, nowUtc);
    }
}

FlowSession &FlowManager::sessionForPacket(const PacketObservation &packet, const FlowKey &key)
{
    auto it = m_sessions.find(key);
    if (it != m_sessions.end()) {
        return it.value();
    }

    FlowSession session;
    session.key = key;
    session.startTimeUtc = packet.timestampUtc.isValid()
            ? packet.timestampUtc.toUTC()
            : QDateTime::currentDateTimeUtc();
    session.lastSeenUtc = session.startTimeUtc;
    session.directionFirstSeen = packet.direction;
    session.ipv6 = key.ipVersion == 6;
    session.loopback = packet.loopback || key.localIp.isLoopback() || key.remoteIp.isLoopback();

    auto inserted = m_sessions.insert(key, session);
    return inserted.value();
}

void FlowManager::closeFlow(const FlowKey &key, const QString &closeReason, const QDateTime &endTimeUtc)
{
    auto it = m_sessions.find(key);
    if (it == m_sessions.end()) {
        return;
    }

    const FlowSession session = it.value();
    m_sessions.erase(it);
    emit sessionClosed(makeRecord(session, closeReason, endTimeUtc.isValid() ? endTimeUtc.toUTC() : QDateTime::currentDateTimeUtc()));
}

NetworkSessionRecord FlowManager::makeRecord(const FlowSession &session, const QString &closeReason, const QDateTime &endTimeUtc)
{
    NetworkSessionRecord record;
    record.username = m_username;
    record.flowId = session.flowId;
    record.startTimeUtc = session.startTimeUtc;
    record.endTimeUtc = endTimeUtc;
    record.key = session.key;
    record.process = session.process;
    record.remoteHost = m_dnsCache.lookup(session.key.remoteIp, session.process.pid, session.startTimeUtc);
    record.appProtocol = ProtocolInferencer::infer(session.key.transport, session.key.localPort, session.key.remotePort);
    record.bytesSentTotal = session.bytesSentTotal;
    record.bytesReceivedTotal = session.bytesReceivedTotal;
    record.payloadBytesSent = session.payloadBytesSent;
    record.payloadBytesReceived = session.payloadBytesReceived;
    record.packetsSent = session.packetsSent;
    record.packetsReceived = session.packetsReceived;
    record.closeReason = closeReason;
    record.ipv6 = session.ipv6;
    record.loopback = session.loopback;
    return record;
}

void FlowManager::applyProcess(FlowSession &session, quint32 pid, const QString &name, const QString &path,
                               const QString &source, const QString &confidence)
{
    if (pid == 0) {
        return;
    }

    if (processConfidenceRank(confidence) < processConfidenceRank(session.process.confidence)) {
        return;
    }

    session.process.pid = pid;
    if (!name.isEmpty()) {
        session.process.name = name;
    }
    if (!path.isEmpty()) {
        session.process.path = path;
    }
    session.process.source = source;
    session.process.confidence = confidence;
}

} // namespace Network
