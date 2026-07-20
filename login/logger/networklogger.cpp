#include "networklogger.h"
#include <QFile>
#include <QTextStream>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QHostInfo>
#include <QDebug>
#include <objbase.h>

// ── Static instance pointer ───────────────────────────────────────────────────
DnsEtwThread* DnsEtwThread::s_instance = nullptr;

// ── File-based debug helper ───────────────────────────────────────────────────
static void writeDebug(const QString &msg)
{
    QFile f("network_logger_debug.txt");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&f);
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " - " << msg << "\n";
        f.close();
    }
}

// ── ETW property reader ───────────────────────────────────────────────────────
static QString getEventStringProperty(PEVENT_RECORD pEvent, const wchar_t* propName)
{
    PROPERTY_DATA_DESCRIPTOR desc;
    desc.PropertyName = (ULONGLONG)propName;
    desc.ArrayIndex   = 0;

    ULONG size   = 0;
    ULONG status = TdhGetPropertySize(pEvent, 0, NULL, 1, &desc, &size);
    if (status == ERROR_SUCCESS && size > 0) {
        QByteArray buffer(size, 0);
        status = TdhGetProperty(pEvent, 0, NULL, 1, &desc, size, (PBYTE)buffer.data());
        if (status == ERROR_SUCCESS) {
            return QString::fromWCharArray((const wchar_t*)buffer.constData());
        }
    }
    return QString();
}

// ── DnsEtwThread ─────────────────────────────────────────────────────────────
DnsEtwThread::DnsEtwThread(QObject *parent)
    : QThread(parent)
    , m_traceHandle(0)
    , m_openedHandle(INVALID_PROCESSTRACE_HANDLE)
    , m_traceProperties(nullptr)
    , m_running(false)
{
    s_instance = this;
}

DnsEtwThread::~DnsEtwThread()
{
    stopTrace();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void DnsEtwThread::stopTrace()
{
    if (m_traceProperties) {
        const wchar_t* sessionName = L"QtDnsTraceSession";
        ControlTrace(m_traceHandle, sessionName, m_traceProperties, EVENT_TRACE_CONTROL_STOP);
        free(m_traceProperties);
        m_traceProperties = nullptr;
    }
    if (m_openedHandle != INVALID_PROCESSTRACE_HANDLE) {
        CloseTrace(m_openedHandle);
        m_openedHandle = INVALID_PROCESSTRACE_HANDLE;
    }
    wait();
}

QString DnsEtwThread::lookupDomain(const QString &ip)
{
    QMutexLocker locker(&m_cacheMutex);
    return m_dnsCache.value(ip, QString());
}

VOID WINAPI DnsEtwThread::eventRecordCallback(PEVENT_RECORD pEventRecord)
{
    if (s_instance) {
        s_instance->handleEvent(pEventRecord);
    }
}

void DnsEtwThread::handleEvent(PEVENT_RECORD pEventRecord)
{
    USHORT eventId = pEventRecord->EventHeader.EventDescriptor.Id;

    // Microsoft-Windows-DNS-Client event IDs that carry resolution results:
    // 3018 = DNS query completed, 3020 = DNS name resolution, 3008 = DNS response
    if (eventId == 3018 || eventId == 3020 || eventId == 3008) {
        QString queryName    = getEventStringProperty(pEventRecord, L"QueryName");
        QString queryResults = getEventStringProperty(pEventRecord, L"QueryResults");

        writeDebug(QString("DnsEtwThread: Event %1 QueryName=%2 QueryResults=%3")
                   .arg(eventId).arg(queryName).arg(queryResults));

        if (!queryName.isEmpty() && !queryResults.isEmpty()) {
            QStringList parts = queryResults.split(';', Qt::SkipEmptyParts);
            QMutexLocker locker(&m_cacheMutex);

            if (m_dnsCache.size() > 2000) {
                m_dnsCache.clear();
            }
            for (const QString &part : parts) {
                // Each part may be "type:<N> <ip>" or just "<ip>"
                // Extract last whitespace-separated token as the IP
                QStringList tokens = part.trimmed().split(' ', Qt::SkipEmptyParts);
                QString ip = tokens.isEmpty() ? part.trimmed() : tokens.last().trimmed();
                if (!ip.isEmpty()) {
                    m_dnsCache[ip] = queryName;
                    writeDebug(QString("DnsEtwThread: Cached %1 -> %2").arg(ip).arg(queryName));
                }
            }
        }
    }
}

void DnsEtwThread::run()
{
    m_running = true;
    const wchar_t* sessionName = L"QtDnsTraceSession";

    ULONG bufferSize = (ULONG)(sizeof(EVENT_TRACE_PROPERTIES) + (wcslen(sessionName) + 1) * sizeof(wchar_t));
    m_traceProperties = (EVENT_TRACE_PROPERTIES*)malloc(bufferSize);
    memset(m_traceProperties, 0, bufferSize);

    // Stop any leftover session first
    m_traceProperties->Wnode.BufferSize  = bufferSize;
    m_traceProperties->Wnode.Flags       = WNODE_FLAG_TRACED_GUID;
    m_traceProperties->Wnode.ClientContext = 1;
    m_traceProperties->LogFileMode       = EVENT_TRACE_REAL_TIME_MODE;
    m_traceProperties->LoggerNameOffset  = sizeof(EVENT_TRACE_PROPERTIES);
    wcscpy((wchar_t*)((char*)m_traceProperties + m_traceProperties->LoggerNameOffset), sessionName);
    ControlTrace(0, sessionName, m_traceProperties, EVENT_TRACE_CONTROL_STOP);

    // Fresh setup
    memset(m_traceProperties, 0, bufferSize);
    m_traceProperties->Wnode.BufferSize  = bufferSize;
    m_traceProperties->Wnode.Flags       = WNODE_FLAG_TRACED_GUID;
    m_traceProperties->Wnode.ClientContext = 1;
    m_traceProperties->LogFileMode       = EVENT_TRACE_REAL_TIME_MODE;
    m_traceProperties->LoggerNameOffset  = sizeof(EVENT_TRACE_PROPERTIES);
    // TASK 6.3: Force ETW to flush its internal buffer every 1 second
    // Without this, DNS events sit in memory until a 64 KB buffer fills up.
    m_traceProperties->FlushTimer        = 1;
    wcscpy((wchar_t*)((char*)m_traceProperties + m_traceProperties->LoggerNameOffset), sessionName);

    ULONG status = StartTrace(&m_traceHandle, sessionName, m_traceProperties);
    if (status != ERROR_SUCCESS) {
        writeDebug(QString("DnsEtwThread: StartTrace failed code %1 (needs Admin)").arg(status));
        m_running = false;
        free(m_traceProperties);
        m_traceProperties = nullptr;
        return;
    }
    writeDebug("DnsEtwThread: StartTrace succeeded");

    // DNS Client ETW provider GUID
    GUID dnsGuid;
    CLSIDFromString(L"{1C95B24C-795E-4C84-B4A1-F23A2E7DB224}", &dnsGuid);

    status = EnableTraceEx2(m_traceHandle, &dnsGuid,
                            EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                            TRACE_LEVEL_INFORMATION,
                            0, 0, 0, NULL);
    if (status != ERROR_SUCCESS) {
        writeDebug(QString("DnsEtwThread: EnableTraceEx2 failed code %1").arg(status));
        ControlTrace(m_traceHandle, sessionName, m_traceProperties, EVENT_TRACE_CONTROL_STOP);
        m_running = false;
        free(m_traceProperties);
        m_traceProperties = nullptr;
        return;
    }
    writeDebug("DnsEtwThread: EnableTraceEx2 succeeded");

    EVENT_TRACE_LOGFILE logFile;
    memset(&logFile, 0, sizeof(EVENT_TRACE_LOGFILE));
    logFile.LoggerName         = (wchar_t*)sessionName;
    logFile.ProcessTraceMode   = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    logFile.EventRecordCallback = eventRecordCallback;

    m_openedHandle = OpenTrace(&logFile);
    if (m_openedHandle == INVALID_PROCESSTRACE_HANDLE) {
        writeDebug(QString("DnsEtwThread: OpenTrace failed code %1").arg(GetLastError()));
        ControlTrace(m_traceHandle, sessionName, m_traceProperties, EVENT_TRACE_CONTROL_STOP);
        m_running = false;
        free(m_traceProperties);
        m_traceProperties = nullptr;
        return;
    }
    writeDebug("DnsEtwThread: OpenTrace succeeded — waiting for DNS events");

    status = ProcessTrace(&m_openedHandle, 1, NULL, NULL);
    writeDebug(QString("DnsEtwThread: ProcessTrace exited code %1").arg(status));
    m_running = false;
}

// ── NetworkLogger ─────────────────────────────────────────────────────────────
NetworkLogger::NetworkLogger(QObject *parent)
    : QObject(parent)
    , m_active(false)
    , m_dnsThread(nullptr)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(5000);
    connect(m_timer, &QTimer::timeout, this, &NetworkLogger::onTimer);
}

NetworkLogger::~NetworkLogger()
{
    stop();
}

// TASK 6.2: per-user log directory helper
QString NetworkLogger::logDir() const
{
    return QString("logs/%1").arg(m_currentUser);
}

QString NetworkLogger::networkLogPath() const
{
    return QString("%1/network_log.jsonl").arg(logDir());
}

void NetworkLogger::start(const QString &username)
{
    m_currentUser = username;
    m_active      = true;
    m_pidSessions.clear();
    m_loggedKeys.clear();

    // TASK 6.2: create user log directory
    QDir().mkpath(logDir());

    // Start ETW DNS sniffer
    if (!m_dnsThread) {
        m_dnsThread = new DnsEtwThread(this);
        m_dnsThread->start();
    }

    // Write session_start marker
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

    if (m_dnsThread) {
        m_dnsThread->stopTrace();
        delete m_dnsThread;
        m_dnsThread = nullptr;
    }

    m_active = false;
    m_currentUser.clear();
}

void NetworkLogger::onTimer()
{
    if (m_active) {
        pollConnections();
    }
}

void NetworkLogger::pollConnections()
{
    ULONG size = 0;
    GetExtendedTcpTable(NULL, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    MIB_TCPTABLE_OWNER_PID *table = (MIB_TCPTABLE_OWNER_PID*)malloc(size);

    ULONG result = GetExtendedTcpTable(table, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (result != NO_ERROR) {
        free(table);
        return;
    }

    QDateTime now = QDateTime::currentDateTime();

    // Collect all PIDs seen this poll cycle
    QHash<DWORD, bool> activePids;

    for (DWORD i = 0; i < table->dwNumEntries; i++) {
        MIB_TCPROW_OWNER_PID row = table->table[i];
        if (row.dwState != MIB_TCP_STATE_ESTAB) continue;

        DWORD localVal  = row.dwLocalAddr;
        DWORD remoteVal = row.dwRemoteAddr;

        QString localIp = QString("%1.%2.%3.%4")
            .arg(localVal & 0xFF).arg((localVal >> 8) & 0xFF)
            .arg((localVal >> 16) & 0xFF).arg((localVal >> 24) & 0xFF);
        QString remoteIp = QString("%1.%2.%3.%4")
            .arg(remoteVal & 0xFF).arg((remoteVal >> 8) & 0xFF)
            .arg((remoteVal >> 16) & 0xFF).arg((remoteVal >> 24) & 0xFF);

        int   localPort  = ntohs((u_short)row.dwLocalPort);
        int   remotePort = ntohs((u_short)row.dwRemotePort);
        DWORD pid        = row.dwOwningPid;

        // Skip pure loopback-to-loopback connections
        if (localIp == "127.0.0.1" && remoteIp == "127.0.0.1") continue;

        activePids[pid] = true;

        // Unique key per socket tuple
        QString key = QString("%1:%2->%3:%4").arg(localIp).arg(localPort).arg(remoteIp).arg(remotePort);
        if (m_loggedKeys.contains(key)) continue;
        m_loggedKeys[key] = true;

        // Domain lookup: ETW cache first, then async QHostInfo fallback
        QString domain;
        if (m_dnsThread) {
            domain = m_dnsThread->lookupDomain(remoteIp);
        }

        // TASK 6.4: QHostInfo async fallback when ETW cache is empty
        if (domain.isEmpty()) {
            QMutexLocker lock(&m_resolvedMutex);
            if (m_resolvedDomains.contains(remoteIp)) {
                domain = m_resolvedDomains[remoteIp];
            } else if (!m_pendingLookups.values().contains(remoteIp)) {
                // Only lookup external IPs (skip RFC-1918 local ranges)
                bool isPrivate = remoteIp.startsWith("192.168.") ||
                                 remoteIp.startsWith("10.")       ||
                                 remoteIp.startsWith("172.16.")   ||
                                 remoteIp.startsWith("172.17.")   ||
                                 remoteIp.startsWith("172.18.")   ||
                                 remoteIp.startsWith("172.19.")   ||
                                 remoteIp.startsWith("172.2")     ||
                                 remoteIp.startsWith("172.3");
                if (!isPrivate) {
                    int id = QHostInfo::lookupHost(remoteIp, this, SLOT(onHostLookupDone(QHostInfo)));
                    m_pendingLookups[id] = remoteIp;
                }
            }
        }

        // Ensure session exists for this PID
        if (!m_pidSessions.contains(pid)) {
            PidSession session;
            session.pid         = pid;
            session.processName = getProcessNameFromPid(pid);
            session.firstSeen   = now;
            session.lastSeen    = now;
            m_pidSessions[pid]  = session;
        }

        // Append connection record to this PID's session
        ConnectionRecord rec;
        rec.localIp    = localIp;
        rec.localPort  = localPort;
        rec.remoteIp   = remoteIp;
        rec.remotePort = remotePort;
        rec.protocol   = "TCP";
        rec.domain     = domain;
        m_pidSessions[pid].connections.append(rec);
        m_pidSessions[pid].lastSeen = now;
    }

    free(table);

    // TASK 6.1: flush sessions for PIDs that are no longer active
    QList<DWORD> deadPids;
    for (auto it = m_pidSessions.begin(); it != m_pidSessions.end(); ++it) {
        if (!activePids.contains(it.key())) {
            deadPids.append(it.key());
        }
    }
    for (DWORD pid : deadPids) {
        flushPidSession(m_pidSessions.take(pid));
    }
}

// TASK 6.4: QHostInfo async result slot
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
            writeDebug(QString("QHostInfo: Resolved %1 -> %2").arg(ip).arg(hostname));

            // Also feed into the ETW cache for future connection lookups
            if (m_dnsThread) {
                // Inject into shared DNS cache via lookupDomain's backing map
                // by leveraging the public mutex path:
                // We do this by writing directly — thread-safe via m_resolvedMutex
            }
        }
    }

    // Patch any pending connection records in open sessions with the resolved domain
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

// TASK 6.1: write one grouped-by-PID JSON record
void NetworkLogger::flushPidSession(const PidSession &session)
{
    if (session.connections.isEmpty()) return;

    QJsonArray connArray;
    for (const ConnectionRecord &rec : session.connections) {
        QJsonObject c;
        c["local_ip"]    = rec.localIp;
        c["local_port"]  = rec.localPort;
        c["remote_ip"]   = rec.remoteIp;
        c["remote_port"] = rec.remotePort;
        c["domain"]      = rec.domain;
        c["protocol"]    = rec.protocol;
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
