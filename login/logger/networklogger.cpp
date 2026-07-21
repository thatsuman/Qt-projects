#include "networklogger.h"
#include <QFile>
#include <QTextStream>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QHostInfo>
#include <QDebug>
#include <objbase.h>
#include <psapi.h>

static void writeDebug(const QString &msg)
{
    QFile f("network_logger_debug.txt");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&f);
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " - " << msg << "\n";
        f.close();
    }
}

// ── NetworkLogger ─────────────────────────────────────────────────────────────
NetworkLogger::NetworkLogger(QObject *parent)
    : QObject(parent)
    , m_active(false)
    , m_etwSession(nullptr)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(5000);
    connect(m_timer, &QTimer::timeout, this, &NetworkLogger::onTimer);
}

NetworkLogger::~NetworkLogger()
{
    stop();
}

QString NetworkLogger::logDir() const
{
    return QString("logs/%1").arg(m_currentUser);
}

QString NetworkLogger::networkLogPath() const
{
    return QString("%1/network_log.jsonl").arg(logDir());
}

QString NetworkLogger::formatBytes(quint64 bytes)
{
    double kb = bytes / 1024.0;
    double mb = kb / 1024.0;
    if (mb >= 1.0) {
        return QString("%1 MB").arg(mb, 0, 'f', 2);
    } else if (kb >= 0.1) {
        return QString("%1 KB").arg(kb, 0, 'f', 1);
    }
    return QString("%1 Bytes").arg(bytes);
}

void NetworkLogger::start(const QString &username)
{
    m_currentUser = username;
    m_active      = true;
    m_pidSessions.clear();

    QDir().mkpath(logDir());

    // Fresh initialization of ETW Session
    if (!m_etwSession) {
        m_etwSession = new EtwTraceSession(this);
        connect(m_etwSession, &EtwTraceSession::tcpEventOccurred, this, &NetworkLogger::onEtwTcpEvent);
        connect(m_etwSession, &EtwTraceSession::httpEventOccurred, this, &NetworkLogger::onEtwHttpEvent);
        
        bool ok = m_etwSession->startSession();
        if (!ok) {
            writeDebug("NetworkLogger: Failed to start ETW Session (requires Administrator elevation).");
        } else {
            writeDebug("NetworkLogger: Started real-time ETW trace session successfully.");
        }
    }

    // Session marker
    QFile logFile(networkLogPath());
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QJsonObject obj;
        obj["type"]      = "session_start";
        obj["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        obj["username"]  = m_currentUser;
        QTextStream(&logFile) << QJsonDocument(obj).toJson(QJsonDocument::Compact) << "\n";
    }

    m_timer->start();
}

void NetworkLogger::stop()
{
    if (!m_active) return;

    m_timer->stop();
    flushAllSessions();

    if (m_etwSession) {
        m_etwSession->stopSession();
        delete m_etwSession;
        m_etwSession = nullptr;
    }

    m_active = false;
    m_currentUser.clear();
}

void NetworkLogger::onTimer()
{
    if (!m_active) return;

    // Check which PIDs are still running. Flush/terminate sessions for dead processes.
    QList<DWORD> deadPids;
    for (auto it = m_pidSessions.begin(); it != m_pidSessions.end(); ++it) {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, it.key());
        if (hProc == NULL) {
            deadPids.append(it.key());
        } else {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(hProc, &exitCode) && exitCode != STILL_ACTIVE) {
                deadPids.append(it.key());
            }
            CloseHandle(hProc);
        }
    }

    for (DWORD pid : deadPids) {
        flushPidSession(m_pidSessions.take(pid));
    }
}

void NetworkLogger::onEtwTcpEvent(const EtwTcpEvent &event)
{
    if (!m_active) return;

    QDateTime now = QDateTime::currentDateTime();

    // Ensure session exists for this PID
    if (!m_pidSessions.contains(event.pid)) {
        PidSession session;
        session.pid         = event.pid;
        session.processName = getProcessNameFromPid(event.pid);
        session.firstSeen   = now;
        session.lastSeen    = now;
        m_pidSessions[event.pid] = session;
    }

    PidSession &session = m_pidSessions[event.pid];
    session.lastSeen = now;

    // Consolidation Logic: Check last connection record for this PID session
    bool merged = false;
    if (!session.connections.isEmpty()) {
        ConnectionRecord &lastRec = session.connections.last();
        // Consolidate if Local IP and Remote IP match identically (ignoring ephemeral ports)
        if (lastRec.localIp == event.localIp && lastRec.remoteIp == event.remoteIp) {
            if (!lastRec.localPorts.contains(event.localPort)) {
                lastRec.localPorts.append(event.localPort);
            }
            if (!lastRec.remotePorts.contains(event.remotePort)) {
                lastRec.remotePorts.append(event.remotePort);
            }
            if (event.isSend) {
                lastRec.bytesSent += event.bytesTransferred;
            } else {
                lastRec.bytesReceived += event.bytesTransferred;
            }
            lastRec.lastSeen = now;
            lastRec.connectionCount++;
            merged = true;

            emit networkActivityOccurred(QString("[CONSOLIDATED] %1 -> %2 | Ports: %3 -> %4 | Protocol: %5 | Sent: %6 | Recv: %7 | Count: %8")
                                             .arg(lastRec.localIp).arg(lastRec.remoteIp)
                                             .arg(event.localPort).arg(event.remotePort)
                                             .arg(lastRec.protocol)
                                             .arg(formatBytes(lastRec.bytesSent))
                                             .arg(formatBytes(lastRec.bytesReceived))
                                             .arg(lastRec.connectionCount));
        }
    }

    if (!merged) {
        ConnectionRecord rec;
        rec.localIp = event.localIp;
        rec.remoteIp = event.remoteIp;
        rec.localPorts.append(event.localPort);
        rec.remotePorts.append(event.remotePort);
        rec.firstSeen = now;
        rec.lastSeen = now;
        if (event.isSend) {
            rec.bytesSent = event.bytesTransferred;
        } else {
            rec.bytesReceived = event.bytesTransferred;
        }
        rec.connectionCount = 1;

        // Determine protocol
        ConnectionTuple tuple = {event.localIp, event.localPort, event.remoteIp, event.remotePort, event.pid};
        NetworkProtocol proto = m_classifier.classify(tuple);
        rec.protocol = protocolToString(proto);

        // Async DNS lookup
        {
            QMutexLocker lock(&m_resolvedMutex);
            rec.domain = m_resolvedDomains.value(event.remoteIp);
        }

        if (rec.domain.isEmpty() && !event.remoteIp.isEmpty()) {
            bool isPrivate = event.remoteIp.startsWith("192.168.") ||
                             event.remoteIp.startsWith("10.")       ||
                             event.remoteIp.startsWith("172.16.")   ||
                             event.remoteIp.startsWith("172.17.")   ||
                             event.remoteIp.startsWith("172.18.")   ||
                             event.remoteIp.startsWith("172.19.")   ||
                             event.remoteIp.startsWith("172.2")     ||
                             event.remoteIp.startsWith("172.3");
            if (!isPrivate) {
                QMutexLocker lock(&m_resolvedMutex);
                if (!m_pendingLookups.values().contains(event.remoteIp)) {
                    int id = QHostInfo::lookupHost(event.remoteIp, this, SLOT(onHostLookupDone(QHostInfo)));
                    m_pendingLookups[id] = event.remoteIp;
                }
            }
        }
        session.connections.append(rec);

        emit networkActivityOccurred(QString("[NEW] %1 -> %2 | Port: %3 -> %4 | Protocol: %5 | Sent: %6 | Recv: %7")
                                         .arg(rec.localIp).arg(rec.remoteIp)
                                         .arg(event.localPort).arg(event.remotePort)
                                         .arg(rec.protocol)
                                         .arg(formatBytes(rec.bytesSent))
                                         .arg(formatBytes(rec.bytesReceived)));
    }
}

void NetworkLogger::onEtwHttpEvent(const EtwHttpEvent &event)
{
    if (!m_active) return;

    if (m_pidSessions.contains(event.pid)) {
        PidSession &session = m_pidSessions[event.pid];
        // Scan backwards to update the classification of matching connection record
        for (int i = session.connections.size() - 1; i >= 0; --i) {
            ConnectionRecord &rec = session.connections[i];
            if (event.url.contains(rec.remoteIp) || (!rec.domain.isEmpty() && event.url.contains(rec.domain))) {
                QString oldProtocol = rec.protocol;
                rec.protocol = event.protocol;
                
                // Update classification registry
                for (int lp : rec.localPorts) {
                    for (int rp : rec.remotePorts) {
                        ConnectionTuple tuple = {rec.localIp, lp, rec.remoteIp, rp, event.pid};
                        m_classifier.recordTransition(tuple, event.protocol == "WebSocket" ? NetworkProtocol::WEBSOCKET : NetworkProtocol::REST_API);
                    }
                }
                
                emit networkActivityOccurred(QString("[UPGRADE] %1 -> %2 | Protocol transition: %3 -> %4 | Target: %5")
                                                 .arg(rec.localIp).arg(rec.remoteIp)
                                                 .arg(oldProtocol).arg(rec.protocol)
                                                 .arg(event.url));
                break;
            }
        }
    }
}

void NetworkLogger::onHostLookupDone(const QHostInfo &info)
{
    int id = info.lookupId();
    QString ip;
    {
        QMutexLocker lock(&m_resolvedMutex);
        ip = m_pendingLookups.take(id);
        if (ip.isEmpty()) return;

        QString hostname = info.hostName();
        if (!hostname.isEmpty() && hostname != ip) {
            m_resolvedDomains[ip] = hostname;
        }
    }

    if (!ip.isEmpty()) {
        QString hostname;
        {
            QMutexLocker lock(&m_resolvedMutex);
            hostname = m_resolvedDomains.value(ip);
        }
        if (!hostname.isEmpty()) {
            for (auto &session : m_pidSessions) {
                for (auto &conn : session.connections) {
                    if (conn.remoteIp == ip && conn.domain.isEmpty()) {
                        conn.domain = hostname;
                    }
                }
            }
        }
    }
}

void NetworkLogger::flushPidSession(const PidSession &session)
{
    if (session.connections.isEmpty()) return;

    QJsonArray connArray;
    for (const ConnectionRecord &rec : session.connections) {
        QJsonObject c;
        c["local_ip"]    = rec.localIp;
        c["remote_ip"]   = rec.remoteIp;

        QJsonArray localPortsArr;
        for (int p : rec.localPorts) localPortsArr.append(p);
        c["local_ports"] = localPortsArr;

        QJsonArray remotePortsArr;
        for (int p : rec.remotePorts) remotePortsArr.append(p);
        c["remote_ports"] = remotePortsArr;

        c["domain"]      = rec.domain;
        c["protocol"]    = rec.protocol;
        c["bytes_sent"]  = (qint64)rec.bytesSent;
        c["bytes_received"] = (qint64)rec.bytesReceived;
        c["bytes_sent_formatted"] = formatBytes(rec.bytesSent);
        c["bytes_received_formatted"] = formatBytes(rec.bytesReceived);
        c["connection_count"] = rec.connectionCount;
        connArray.append(c);
    }

    QJsonObject record;
    record["type"]              = "network_session";
    record["timestamp_start"]   = session.firstSeen.toString("yyyy-MM-dd hh:mm:ss");
    record["timestamp_end"]     = session.lastSeen.toString("yyyy-MM-dd hh:mm:ss");
    record["pid"]               = (qint64)session.pid;
    record["process_name"]      = session.processName;
    record["username"]          = m_currentUser;
    record["connections_count"] = session.connections.size();
    record["connections"]       = connArray;

    QFile logFile(networkLogPath());
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream(&logFile) << QJsonDocument(record).toJson(QJsonDocument::Compact) << "\n";
    }
}

void NetworkLogger::flushAllSessions()
{
    for (const PidSession &session : m_pidSessions) {
        flushPidSession(session);
    }
    m_pidSessions.clear();
}

QString NetworkLogger::getProcessNameFromPid(DWORD pid)
{
    QString processName = "unknown";
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProc != NULL) {
        wchar_t procBuf[256];
        if (GetModuleBaseName(hProc, NULL, procBuf, 256) > 0) {
            processName = QString::fromWCharArray(procBuf);
        }
        CloseHandle(hProc);
    }
    return processName;
}
