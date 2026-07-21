#include "EtwTraceSession.h"
#include "etw_providers.h"
#include <QDebug>
#include <objbase.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <tdh.h>

EtwTraceSession* EtwTraceSession::s_instance = nullptr;

static void writeEtwDebug(const QString &msg);

// Helper: read a property as raw QByteArray via TDH
static QByteArray getEventProperty(PEVENT_RECORD pEvent, const wchar_t* propName)
{
    PROPERTY_DATA_DESCRIPTOR desc = {};
    desc.PropertyName = (ULONGLONG)propName;
    desc.ArrayIndex   = 0;
    desc.Reserved     = 0;

    ULONG size   = 0;
    ULONG status = TdhGetPropertySize(pEvent, 0, NULL, 1, &desc, &size);
    if (status == ERROR_SUCCESS && size > 0) {
        QByteArray buffer(size, 0);
        status = TdhGetProperty(pEvent, 0, NULL, 1, &desc, size, (PBYTE)buffer.data());
        if (status == ERROR_SUCCESS) {
            return buffer;
        }
    }
    return QByteArray();
}

// Helper: parse IP and port from a SOCKADDR binary structure
static void parseSockAddr(const QByteArray &bytes, QString &ip, int &port)
{
    if (bytes.size() < (int)sizeof(ADDRESS_FAMILY)) return;
    
    const sockaddr* addr = reinterpret_cast<const sockaddr*>(bytes.constData());
    if (addr->sa_family == AF_INET) {
        if (bytes.size() < (int)sizeof(sockaddr_in)) return;
        const sockaddr_in* ipv4 = reinterpret_cast<const sockaddr_in*>(bytes.constData());
        char ipBuf[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, const_cast<in_addr*>(&(ipv4->sin_addr)), ipBuf, INET_ADDRSTRLEN);
        ip = QString::fromLatin1(ipBuf);
        port = ntohs(ipv4->sin_port);
    } else if (addr->sa_family == AF_INET6) {
        if (bytes.size() < (int)sizeof(sockaddr_in6)) return;
        const sockaddr_in6* ipv6 = reinterpret_cast<const sockaddr_in6*>(bytes.constData());
        char ipBuf[INET6_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET6, const_cast<in6_addr*>(&(ipv6->sin6_addr)), ipBuf, INET6_ADDRSTRLEN);
        ip = QString::fromLatin1(ipBuf);
        port = ntohs(ipv6->sin6_port);
    }
}

// Helper: extract integer value from raw property bytes
template<typename T>
static T getPropertyInt(const QByteArray &bytes, T defaultValue = 0)
{
    if (bytes.size() >= (int)sizeof(T)) {
        return *reinterpret_cast<const T*>(bytes.constData());
    }
    return defaultValue;
}

EtwTraceSession::EtwTraceSession(QObject *parent)
    : QThread(parent)
    , m_traceHandle(0)
    , m_openedHandle(INVALID_PROCESSTRACE_HANDLE)
    , m_traceProperties(nullptr)
    , m_running(false)
{
    s_instance = this;
}

EtwTraceSession::~EtwTraceSession()
{
    writeEtwDebug("EtwTraceSession destructor called");
    stopSession();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

bool EtwTraceSession::isUserAdminOrPerformanceLogUser()
{
    BOOL fRet = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            fRet = elevation.TokenIsElevated;
        }
    }
    if (hToken) {
        CloseHandle(hToken);
    }
    return fRet != FALSE;
}

bool EtwTraceSession::startSession()
{
    if (!isUserAdminOrPerformanceLogUser()) {
        qWarning() << "EtwTraceSession: Insufficient privileges to launch ETW trace session.";
        return false;
    }

    const wchar_t* sessionName = L"QtNetworkEtwTraceSession";

    // Allocate trace session properties
    ULONG bufferSize = (ULONG)(sizeof(EVENT_TRACE_PROPERTIES) + (wcslen(sessionName) + 1) * sizeof(wchar_t));
    m_traceProperties = (EVENT_TRACE_PROPERTIES*)malloc(bufferSize);
    memset(m_traceProperties, 0, bufferSize);

    m_traceProperties->Wnode.BufferSize  = bufferSize;
    m_traceProperties->Wnode.Flags       = WNODE_FLAG_TRACED_GUID;
    m_traceProperties->Wnode.ClientContext = 1;
    m_traceProperties->LogFileMode       = EVENT_TRACE_REAL_TIME_MODE;
    m_traceProperties->LoggerNameOffset  = sizeof(EVENT_TRACE_PROPERTIES);
    m_traceProperties->FlushTimer        = 1;

    wcscpy((wchar_t*)((char*)m_traceProperties + m_traceProperties->LoggerNameOffset), sessionName);

    // Stop any conflicting session that was left over
    ControlTrace(0, sessionName, m_traceProperties, EVENT_TRACE_CONTROL_STOP);

    // Start trace
    ULONG status = StartTrace(&m_traceHandle, sessionName, m_traceProperties);
    if (status != ERROR_SUCCESS) {
        qWarning() << "EtwTraceSession: StartTrace failed, error code:" << status;
        free(m_traceProperties);
        m_traceProperties = nullptr;
        return false;
    }

    // Enable TCPIP provider
    EnableTraceEx2(m_traceHandle, &TcpIpProviderGuid, EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                   TRACE_LEVEL_VERBOSE, 0xFFFFFFFFFFFFFFFFULL, 0, 0, NULL);

    // Enable WebIO provider
    EnableTraceEx2(m_traceHandle, &WebIoProviderGuid, EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                   TRACE_LEVEL_VERBOSE, 0xFFFFFFFFFFFFFFFFULL, 0, 0, NULL);

    // Enable WinINet provider
    EnableTraceEx2(m_traceHandle, &WinINetProviderGuid, EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                   TRACE_LEVEL_VERBOSE, 0xFFFFFFFFFFFFFFFFULL, 0, 0, NULL);

    // Enable DNS-Client provider
    EnableTraceEx2(m_traceHandle, &DnsClientProviderGuid, EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                   TRACE_LEVEL_VERBOSE, 0xFFFFFFFFFFFFFFFFULL, 0, 0, NULL);

    // Start background event pump
    start();
    return true;
}

void EtwTraceSession::stopSession()
{
    writeEtwDebug("stopSession called");
    m_running = false;
    if (m_traceProperties) {
        const wchar_t* sessionName = L"QtNetworkEtwTraceSession";
        ControlTrace(m_traceHandle, sessionName, m_traceProperties, EVENT_TRACE_CONTROL_STOP);
        free(m_traceProperties);
        m_traceProperties = nullptr;
    }
    if (m_openedHandle != INVALID_PROCESSTRACE_HANDLE) {
        CloseTrace(m_openedHandle);
        m_openedHandle = INVALID_PROCESSTRACE_HANDLE;
    }
    wait(); // Wait for ProcessTrace to exit
}

#include <QFile>
#include <QTextStream>

static void writeEtwDebug(const QString &msg)
{
    QFile f("network_logger_debug.txt");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&f);
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " - [ETW] " << msg << "\n";
        f.close();
    }
}

void EtwTraceSession::run()
{
    m_running = true;
    const wchar_t* sessionName = L"QtNetworkEtwTraceSession";

    writeEtwDebug("ProcessTrace thread starting...");

    EVENT_TRACE_LOGFILE logFile;
    memset(&logFile, 0, sizeof(EVENT_TRACE_LOGFILE));
    logFile.LoggerName         = (wchar_t*)sessionName;
    logFile.ProcessTraceMode   = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    logFile.EventRecordCallback = eventRecordCallback;

    m_openedHandle = OpenTrace(&logFile);
    if (m_openedHandle == INVALID_PROCESSTRACE_HANDLE) {
        DWORD err = GetLastError();
        qWarning() << "EtwTraceSession: OpenTrace failed, error code:" << err;
        writeEtwDebug(QString("OpenTrace failed, error code: %1").arg(err));
        return;
    }

    writeEtwDebug("OpenTrace succeeded, calling ProcessTrace...");

    ULONG status = ProcessTrace(&m_openedHandle, 1, NULL, NULL);
    qDebug() << "EtwTraceSession: ProcessTrace thread exited with status:" << status;
    writeEtwDebug(QString("ProcessTrace thread exited with status: %1").arg(status));
}

VOID WINAPI EtwTraceSession::eventRecordCallback(PEVENT_RECORD pEventRecord)
{
    if (s_instance) {
        s_instance->handleEvent(pEventRecord);
    }
}

void EtwTraceSession::handleEvent(PEVENT_RECORD pEventRecord)
{
    GUID providerId = pEventRecord->EventHeader.ProviderId;
    USHORT eventId  = pEventRecord->EventHeader.EventDescriptor.Id;

    if (providerId == TcpIpProviderGuid) {
        writeEtwDebug(QString("TcpIp EventID %1 received").arg(eventId));

        // TCP / UDP event mapping
        if (eventId == 1033) {
            // TCP Connection Completed
            QByteArray localAddrBytes = getEventProperty(pEventRecord, L"LocalAddress");
            QByteArray remoteAddrBytes = getEventProperty(pEventRecord, L"RemoteAddress");
            QByteArray tcbBytes = getEventProperty(pEventRecord, L"Tcb");
            QByteArray pidBytes = getEventProperty(pEventRecord, L"ProcessId");

            writeEtwDebug(QString("Event 1033 sizes: localAddr=%1, remoteAddr=%2, tcb=%3, pid=%4")
                          .arg(localAddrBytes.size())
                          .arg(remoteAddrBytes.size())
                          .arg(tcbBytes.size())
                          .arg(pidBytes.size()));

            if (!localAddrBytes.isEmpty() && !remoteAddrBytes.isEmpty() && !tcbBytes.isEmpty()) {
                quint64 tcb = getPropertyInt<quint64>(tcbBytes, 0);
                DWORD pid = getPropertyInt<DWORD>(pidBytes, 0);
                if (pid == 0) pid = pEventRecord->EventHeader.ProcessId;

                EtwConnection conn;
                parseSockAddr(localAddrBytes, conn.localIp, conn.localPort);
                parseSockAddr(remoteAddrBytes, conn.remoteIp, conn.remotePort);
                conn.pid = pid;

                m_tcpConnections[tcb] = conn;
                writeEtwDebug(QString("Event 1033 mapped TCB: %1 -> local=%2:%3 remote=%4:%5 pid=%6")
                              .arg(tcb).arg(conn.localIp).arg(conn.localPort)
                              .arg(conn.remoteIp).arg(conn.remotePort).arg(conn.pid));

                // Emit connection creation event
                EtwTcpEvent ev;
                ev.localIp = conn.localIp;
                ev.localPort = conn.localPort;
                ev.remoteIp = conn.remoteIp;
                ev.remotePort = conn.remotePort;
                ev.pid = conn.pid;
                ev.bytesTransferred = 0;
                ev.isSend = false;
                ev.timestamp = QDateTime::currentDateTime();
                emit tcpEventOccurred(ev);
            }
        }
        else if (eventId == 1159 || eventId == 1160) {
            // TCP Send Data
            QByteArray tcbBytes = getEventProperty(pEventRecord, L"Tcb");
            QByteArray numBytesBytes = getEventProperty(pEventRecord, L"NumBytes");

            if (!tcbBytes.isEmpty() && !numBytesBytes.isEmpty()) {
                quint64 tcb = getPropertyInt<quint64>(tcbBytes, 0);
                quint32 numBytes = getPropertyInt<quint32>(numBytesBytes, 0);

                if (m_tcpConnections.contains(tcb)) {
                    const EtwConnection &conn = m_tcpConnections[tcb];
                    EtwTcpEvent ev;
                    ev.localIp = conn.localIp;
                    ev.localPort = conn.localPort;
                    ev.remoteIp = conn.remoteIp;
                    ev.remotePort = conn.remotePort;
                    ev.pid = conn.pid;
                    ev.bytesTransferred = numBytes;
                    ev.isSend = true;
                    ev.timestamp = QDateTime::currentDateTime();
                    emit tcpEventOccurred(ev);
                } else {
                    writeEtwDebug(QString("Event %1 send ignored (TCB %2 not mapped)").arg(eventId).arg(tcb));
                }
            }
        }
        else if (eventId == 1074) {
            // TCP Receive Data
            QByteArray tcbBytes = getEventProperty(pEventRecord, L"Tcb");
            QByteArray numBytesBytes = getEventProperty(pEventRecord, L"NumBytes");

            if (!tcbBytes.isEmpty() && !numBytesBytes.isEmpty()) {
                quint64 tcb = getPropertyInt<quint64>(tcbBytes, 0);
                quint32 numBytes = getPropertyInt<quint32>(numBytesBytes, 0);

                if (m_tcpConnections.contains(tcb)) {
                    const EtwConnection &conn = m_tcpConnections[tcb];
                    EtwTcpEvent ev;
                    ev.localIp = conn.localIp;
                    ev.localPort = conn.localPort;
                    ev.remoteIp = conn.remoteIp;
                    ev.remotePort = conn.remotePort;
                    ev.pid = conn.pid;
                    ev.bytesTransferred = numBytes;
                    ev.isSend = false;
                    ev.timestamp = QDateTime::currentDateTime();
                    emit tcpEventOccurred(ev);
                } else {
                    writeEtwDebug(QString("Event 1074 recv ignored (TCB %2 not mapped)").arg(tcb));
                }
            }
        }
        else if (eventId == 1038 || eventId == 1043 || eventId == 1040) {
            // TCP Connection closed
            QByteArray tcbBytes = getEventProperty(pEventRecord, L"Tcb");
            if (!tcbBytes.isEmpty()) {
                quint64 tcb = getPropertyInt<quint64>(tcbBytes, 0);
                m_tcpConnections.remove(tcb);
                writeEtwDebug(QString("Event %1 closed TCB: %2").arg(eventId).arg(tcb));
            }
        }
        else if (eventId == 1169) {
            // UDP Send
            QByteArray localAddrBytes = getEventProperty(pEventRecord, L"LocalSockAddr");
            QByteArray remoteAddrBytes = getEventProperty(pEventRecord, L"RemoteSockAddr");
            QByteArray numBytesBytes = getEventProperty(pEventRecord, L"NumBytes");
            QByteArray pidBytes = getEventProperty(pEventRecord, L"Pid");

            writeEtwDebug(QString("Event 1169 UDP Send sizes: local=%1, remote=%2, numBytes=%3, pid=%4")
                          .arg(localAddrBytes.size())
                          .arg(remoteAddrBytes.size())
                          .arg(numBytesBytes.size())
                          .arg(pidBytes.size()));

            if (!localAddrBytes.isEmpty() && !remoteAddrBytes.isEmpty()) {
                DWORD pid = getPropertyInt<DWORD>(pidBytes, 0);
                if (pid == 0) pid = pEventRecord->EventHeader.ProcessId;
                quint32 numBytes = getPropertyInt<quint32>(numBytesBytes, 0);

                EtwTcpEvent ev;
                parseSockAddr(localAddrBytes, ev.localIp, ev.localPort);
                parseSockAddr(remoteAddrBytes, ev.remoteIp, ev.remotePort);
                ev.pid = pid;
                ev.bytesTransferred = numBytes;
                ev.isSend = true;
                ev.timestamp = QDateTime::currentDateTime();
                emit tcpEventOccurred(ev);
            }
        }
        else if (eventId == 1170) {
            // UDP Receive / Deliver
            QByteArray localAddrBytes = getEventProperty(pEventRecord, L"LocalSockAddr");
            QByteArray remoteAddrBytes = getEventProperty(pEventRecord, L"RemoteSockAddr");
            QByteArray numBytesBytes = getEventProperty(pEventRecord, L"NumBytes");
            QByteArray pidBytes = getEventProperty(pEventRecord, L"Pid");

            writeEtwDebug(QString("Event 1170 UDP Recv sizes: local=%1, remote=%2, numBytes=%3, pid=%4")
                          .arg(localAddrBytes.size())
                          .arg(remoteAddrBytes.size())
                          .arg(numBytesBytes.size())
                          .arg(pidBytes.size()));

            if (!localAddrBytes.isEmpty() && !remoteAddrBytes.isEmpty()) {
                DWORD pid = getPropertyInt<DWORD>(pidBytes, 0);
                if (pid == 0) pid = pEventRecord->EventHeader.ProcessId;
                quint32 numBytes = getPropertyInt<quint32>(numBytesBytes, 0);

                EtwTcpEvent ev;
                parseSockAddr(localAddrBytes, ev.localIp, ev.localPort);
                parseSockAddr(remoteAddrBytes, ev.remoteIp, ev.remotePort);
                ev.pid = pid;
                ev.bytesTransferred = numBytes;
                ev.isSend = false;
                ev.timestamp = QDateTime::currentDateTime();
                emit tcpEventOccurred(ev);
            }
        }
    }
    else if (providerId == WebIoProviderGuid || providerId == WinINetProviderGuid) {
        // Web / HTTP event mapping
        QByteArray urlBytes = getEventProperty(pEventRecord, L"Url");
        if (urlBytes.isEmpty()) {
            urlBytes = getEventProperty(pEventRecord, L"URL");
        }
        QByteArray methodBytes = getEventProperty(pEventRecord, L"Method");
        QByteArray statusBytes = getEventProperty(pEventRecord, L"StatusCode");
        if (statusBytes.isEmpty()) {
            statusBytes = getEventProperty(pEventRecord, L"ResponseCode");
        }
        QByteArray typeBytes   = getEventProperty(pEventRecord, L"ContentType");

        if (!urlBytes.isEmpty()) {
            EtwHttpEvent ev;
            ev.pid        = pEventRecord->EventHeader.ProcessId;
            ev.url        = QString::fromWCharArray((const wchar_t*)urlBytes.constData());
            ev.method     = methodBytes.isEmpty() ? "GET" : QString::fromWCharArray((const wchar_t*)methodBytes.constData());
            ev.statusCode = getPropertyInt<uint32_t>(statusBytes, 200);
            ev.contentType = typeBytes.isEmpty() ? "application/json" : QString::fromWCharArray((const wchar_t*)typeBytes.constData());
            ev.timestamp  = QDateTime::currentDateTime();

            // Detect protocol upgrades (e.g. WebSocket handshake upgrades)
            if (ev.url.startsWith("ws://") || ev.url.startsWith("wss://")) {
                ev.protocol = "WebSocket";
            } else {
                ev.protocol = "REST API";
            }

            emit httpEventOccurred(ev);
        }
    }
}
