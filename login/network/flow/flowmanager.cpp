#include "flowmanager.h"

#include "network/protocol/protocolinferencer.h"

#include <algorithm>

namespace Network {

namespace {

int processConfidenceRank(const QString &confidence)
{
    if (confidence == "high") return 3;
    if (confidence == "medium") return 2;
    if (confidence == "low") return 1;
    return 0;
}

int hostnameConfidenceRank(const QString &confidence)
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
    m_pendingProcessByKey.clear();
    m_recentlyClosedFlows.clear();
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
    const QDateTime packetTimeUtc = packet.timestampUtc.isValid()
            ? packet.timestampUtc.toUTC()
            : QDateTime::currentDateTimeUtc();
    if (shouldSuppressRecentlyClosed(key, packetTimeUtc)) {
        return;
    }

    FlowSession &session = sessionForPacket(packet, key);
    const bool outbound = packet.direction != Direction::Inbound;

    session.lastSeenUtc = packetTimeUtc;
    session.loopback = session.loopback || packet.loopback || key.localIp.isLoopback() || key.remoteIp.isLoopback();
    session.ipv6 = key.ipVersion == 6;
    applyHostname(session, packet.visibleHostname, "tls_sni", "high");
    if (!packet.visibleAlpn.isEmpty()) {
        session.applicationLayerCategory = QString("tls_alpn:%1").arg(packet.visibleAlpn);
    }

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
        m_pendingProcessByKey.remove(observation.key);
        return;
    }

    auto it = m_sessions.find(observation.key);
    if (it != m_sessions.end()) {
        applyProcess(it.value(), observation.pid, QString(), QString(), observation.source, "high");
        return;
    }

    PendingProcessAttribution pending;
    pending.pid = observation.pid;
    pending.source = observation.source;
    pending.confidence = "high";
    pending.observedUtc = observation.timestampUtc.isValid()
            ? observation.timestampUtc.toUTC()
            : QDateTime::currentDateTimeUtc();
    if (pending.pid != 0) {
        m_pendingProcessByKey.insert(observation.key, pending);
    }
}

void FlowManager::handleProcessSnapshot(const ProcessConnectionSnapshot &snapshot)
{
    auto applySnapshot = [this, &snapshot](FlowSession &session) {
        if (snapshot.processCreationTimeUtc.isValid()
                && session.startTimeUtc.isValid()
                && snapshot.processCreationTimeUtc > session.startTimeUtc.addMSecs(1000)) {
            emit errorOccurred(QString("Ignoring process snapshot for PID %1 because creation time is newer than flow start.")
                               .arg(snapshot.pid));
            return;
        }

        applyProcess(session, snapshot.pid, snapshot.processName, snapshot.processPath, snapshot.source, "medium");
    };

    auto it = m_sessions.find(snapshot.key);
    if (it != m_sessions.end()) {
        applySnapshot(it.value());
        return;
    }

    if (snapshot.key.transport != TransportProtocol::Udp
            || !snapshot.key.remoteIp.isNull()
            || snapshot.key.remotePort != 0) {
        return;
    }

    for (FlowSession &session : m_sessions) {
        if (snapshotMatchesUdpLocalSocket(snapshot, session)) {
            applySnapshot(session);
        }
    }
}

void FlowManager::handleDnsObservation(const DnsObservation &observation)
{
    m_dnsCache.addObservation(observation);
    for (const IpAddress &ip : observation.answerIps) {
        emit errorOccurred(QStringLiteral("dns_cache_insert host=%1 ip=%2 pid=%3")
                           .arg(observation.queryName)
                           .arg(ip.toString())
                           .arg(observation.pid));
    }

    if (observation.answerIps.isEmpty()) {
        return;
    }

    for (FlowSession &session : m_sessions) {
        if (observation.answerIps.contains(session.key.remoteIp)) {
            if (!observation.queryName.isEmpty()) {
                applyHostname(session, observation.queryName, "etw_dns", "medium");
            }
            if (observation.pid != 0) {
                applyProcess(session, observation.pid, observation.processName, QString(),
                             observation.source, "low");
            }
        }
    }
}

void FlowManager::flushAll(const QString &closeReason)
{
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();

    QList<FlowSession> sessions = m_sessions.values();
    std::sort(sessions.begin(), sessions.end(), [](const FlowSession &a, const FlowSession &b) {
        if (a.startTimeUtc != b.startTimeUtc) {
            return a.startTimeUtc < b.startTimeUtc;
        }
        return a.lastSeenUtc < b.lastSeenUtc;
    });

    for (const FlowSession &session : sessions) {
        closeFlow(session.key, closeReason, nowUtc);
    }
}

void FlowManager::sweepIdleFlows()
{
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    struct PendingClose {
        FlowKey key;
        QString reason;
        QDateTime startTimeUtc;
    };
    QList<PendingClose> toClose;

    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        const FlowSession &session = it.value();
        const qint64 idleSeconds = session.lastSeenUtc.secsTo(nowUtc);

        QString reason;
        if (session.key.transport == TransportProtocol::Tcp && session.tcpFinSeen
                && session.tcpFinSeenUtc.secsTo(nowUtc) >= m_tcpFinGraceSeconds) {
            reason = "tcp_fin";
        } else if (session.key.transport == TransportProtocol::Tcp && idleSeconds >= m_tcpIdleSeconds) {
            reason = "idle_timeout";
        } else if (session.key.transport == TransportProtocol::Udp && idleSeconds >= m_udpIdleSeconds) {
            reason = "idle_timeout";
        }

        if (!reason.isEmpty()) {
            toClose.append({it.key(), reason, session.startTimeUtc});
        }
    }

    std::sort(toClose.begin(), toClose.end(), [](const PendingClose &a, const PendingClose &b) {
        return a.startTimeUtc < b.startTimeUtc;
    });

    for (const auto &item : toClose) {
        closeFlow(item.key, item.reason, nowUtc);
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
    applyPendingProcess(inserted.value());
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
    const QDateTime closedAtUtc = endTimeUtc.isValid() ? endTimeUtc.toUTC() : QDateTime::currentDateTimeUtc();
    rememberClosedFlow(key, closedAtUtc);
    emit sessionClosed(makeRecord(session, closeReason, closedAtUtc));
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
    const bool isDnsResolverTraffic = (session.key.remotePort == 53 || session.key.localPort == 53);
    const bool isPrivateOrLocalIp = session.key.remoteIp.isPrivateOrLocal();
    HostnameAttribution dnsHost;
    if (!isDnsResolverTraffic && !isPrivateOrLocalIp) {
        dnsHost = m_dnsCache.lookup(session.key.remoteIp, session.process.pid, session.startTimeUtc);
        if (dnsHost.primaryName.isEmpty()) {
            emit errorOccurred(QStringLiteral("dns_lookup_miss ip=%1 pid=%2 reason=no_candidate")
                               .arg(session.key.remoteIp.toString())
                               .arg(session.process.pid));
        } else {
            emit errorOccurred(QStringLiteral("dns_lookup_hit ip=%1 host=%2 confidence=%3 reason=ip_match")
                               .arg(session.key.remoteIp.toString())
                               .arg(dnsHost.primaryName)
                               .arg(dnsHost.confidence));
        }
    }

    if (session.remoteHost.primaryName.isEmpty()) {
        record.remoteHost = dnsHost;
    } else if (dnsHost.primaryName.isEmpty()) {
        record.remoteHost = session.remoteHost;
    } else {
        record.remoteHost = hostnameConfidenceRank(session.remoteHost.confidence) >= hostnameConfidenceRank(dnsHost.confidence)
                ? session.remoteHost
                : dnsHost;
    }

    if (session.key.remoteIp.isLoopback()) {
        record.remoteHost.status = "not_applicable";
        record.remoteHost.reason = "not_applicable_loopback";
    } else if (isPrivateOrLocalIp) {
        record.remoteHost.status = "not_applicable";
        record.remoteHost.reason = "not_applicable_private_ip";
    } else if (isDnsResolverTraffic) {
        record.remoteHost.status = "not_applicable";
        record.remoteHost.reason = "not_applicable_dns_resolver";
    } else if (!record.remoteHost.primaryName.isEmpty()) {
        record.remoteHost.status = "resolved";
        if (record.remoteHost.reason.isEmpty() || record.remoteHost.reason == "no_dns_candidate") {
            record.remoteHost.reason = "dns_cache_hit";
        }
    } else {
        record.remoteHost.status = "unresolved";
        if (record.remoteHost.reason.isEmpty()) {
            record.remoteHost.reason = "no_dns_candidate";
        }
    }

    if (!record.remoteHost.primaryName.isEmpty()) {
        emit errorOccurred(QStringLiteral("dns_assigned remote.hostname=%1 for ip=%2 confidence=%3")
                           .arg(record.remoteHost.primaryName)
                           .arg(record.key.remoteIp.toString())
                           .arg(record.remoteHost.confidence));
    }
    record.appProtocol = ProtocolInferencer::infer(session.key.transport, session.key.localPort, session.key.remotePort);
    record.applicationLayerCategory = inferApplicationLayerCategory(session);
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

    if (session.process.pid == pid) {
        if (!name.isEmpty() && (session.process.name == "unknown" || session.process.name.isEmpty())) {
            session.process.name = name;
        }
        if (!path.isEmpty() && session.process.path.isEmpty()) {
            session.process.path = path;
        }
    }

    const int newRank = processConfidenceRank(confidence);
    const int existingRank = processConfidenceRank(session.process.confidence);
    if (newRank < existingRank) {
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

void FlowManager::applyHostname(FlowSession &session, const QString &hostname, const QString &source, const QString &confidence)
{
    if (hostname.isEmpty()) {
        return;
    }

    if (hostnameConfidenceRank(confidence) < hostnameConfidenceRank(session.remoteHost.confidence)) {
        if (!session.remoteHost.candidates.contains(hostname)) {
            session.remoteHost.candidates.append(hostname);
        }
        return;
    }

    session.remoteHost.primaryName = hostname;
    if (!session.remoteHost.candidates.contains(hostname)) {
        session.remoteHost.candidates.prepend(hostname);
    }
    session.remoteHost.source = source;
    session.remoteHost.confidence = confidence;
}

void FlowManager::applyPendingProcess(FlowSession &session)
{
    auto pending = m_pendingProcessByKey.find(session.key);
    if (pending == m_pendingProcessByKey.end()) {
        return;
    }

    applyProcess(session, pending.value().pid, pending.value().name, pending.value().path,
                 pending.value().source, pending.value().confidence);
    m_pendingProcessByKey.erase(pending);
}

QString FlowManager::inferApplicationLayerCategory(const FlowSession &session) const
{
    if (session.applicationLayerCategory.startsWith("tls_alpn:")) {
        const QString alpn = session.applicationLayerCategory.mid(QString("tls_alpn:").size());
        if (alpn.contains("h2")) {
            return "HTTP/2 over TLS";
        }
        if (alpn.contains("http/1.1")) {
            return "HTTP/1.1 over TLS";
        }
        return QString("TLS ALPN %1").arg(alpn);
    }

    const ProtocolHint hint = ProtocolInferencer::infer(session.key.transport, session.key.localPort, session.key.remotePort);
    if (hint.hint == "HTTPS") {
        return "encrypted_web";
    }
    if (hint.hint == "HTTP") {
        return "cleartext_http";
    }
    if (hint.hint == "QUIC/HTTP3") {
        return "QUIC/HTTP3";
    }
    if (hint.hint == "DNS") {
        return "DNS";
    }
    return hint.hint;
}

bool FlowManager::snapshotMatchesUdpLocalSocket(const ProcessConnectionSnapshot &snapshot, const FlowSession &session) const
{
    const bool localIpMatches = snapshot.key.localIp.isNull()
            || snapshot.key.localIp.isAny()
            || session.key.localIp.isNull()
            || session.key.localIp.isAny()
            || session.key.localIp == snapshot.key.localIp;

    return session.key.transport == TransportProtocol::Udp
        && session.key.ipVersion == snapshot.key.ipVersion
        && localIpMatches
        && session.key.localPort == snapshot.key.localPort;
}

bool FlowManager::shouldSuppressRecentlyClosed(const FlowKey &key, const QDateTime &timestampUtc)
{
    const QDateTime nowUtc = timestampUtc.isValid() ? timestampUtc.toUTC() : QDateTime::currentDateTimeUtc();
    auto it = m_recentlyClosedFlows.begin();
    while (it != m_recentlyClosedFlows.end()) {
        if (it.value().secsTo(nowUtc) > m_closedFlowSuppressSeconds) {
            it = m_recentlyClosedFlows.erase(it);
        } else {
            ++it;
        }
    }

    const auto closed = m_recentlyClosedFlows.constFind(key);
    return closed != m_recentlyClosedFlows.constEnd()
        && closed.value().secsTo(nowUtc) <= m_closedFlowSuppressSeconds;
}

void FlowManager::rememberClosedFlow(const FlowKey &key, const QDateTime &timestampUtc)
{
    m_recentlyClosedFlows.insert(key, timestampUtc.isValid() ? timestampUtc.toUTC() : QDateTime::currentDateTimeUtc());
}

} // namespace Network
