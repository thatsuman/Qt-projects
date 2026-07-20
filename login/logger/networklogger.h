#ifndef NETWORKLOGGER_H
#define NETWORKLOGGER_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QString>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QThread>
#include <QDir>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <tdh.h>
#include <psapi.h>
#include <QHostInfo>

// ── Per-connection record stored in-memory per PID ───────────────────────────
struct ConnectionRecord {
    QString remoteIp;
    int     remotePort;
    QString domain;       // resolved hostname (ETW or QHostInfo)
    QString protocol;
    QString localIp;
    int     localPort;
};

// ── Per-PID session tracking structure ───────────────────────────────────────
struct PidSession {
    DWORD            pid;
    QString          processName;
    QDateTime        firstSeen;
    QDateTime        lastSeen;
    QList<ConnectionRecord> connections;
};

// ── DNS ETW Thread ─────────────────────────────────────────────────────────
class DnsEtwThread : public QThread
{
    Q_OBJECT
public:
    explicit DnsEtwThread(QObject *parent = nullptr);
    ~DnsEtwThread() override;

    void stopTrace();
    QString lookupDomain(const QString &ip);

protected:
    void run() override;

private:
    static VOID WINAPI eventRecordCallback(PEVENT_RECORD pEventRecord);
    void handleEvent(PEVENT_RECORD pEventRecord);

    TRACEHANDLE m_traceHandle;
    TRACEHANDLE m_openedHandle;
    EVENT_TRACE_PROPERTIES *m_traceProperties;
    bool m_running;

    static DnsEtwThread* s_instance;
    QHash<QString, QString> m_dnsCache;
    QMutex m_cacheMutex;
};

// ── Network Logger ──────────────────────────────────────────────────────────
class NetworkLogger : public QObject
{
    Q_OBJECT
public:
    explicit NetworkLogger(QObject *parent = nullptr);
    ~NetworkLogger() override;

    void start(const QString &username);
    void stop();

private slots:
    void onTimer();
    void onHostLookupDone(const QHostInfo &info);

private:
    void pollConnections();
    QString getProcessNameFromPid(DWORD pid);
    void flushPidSession(const PidSession &session);
    void flushAllSessions();

    QString logDir() const;          // returns "logs/<username>"
    QString networkLogPath() const;  // returns full path to network_log.jsonl

    QTimer  *m_timer;
    QString  m_currentUser;
    bool     m_active;
    DnsEtwThread *m_dnsThread;

    // PID → active session (connections still open)
    QHash<DWORD, PidSession> m_pidSessions;

    // Simple dedup: track connection keys we already logged
    QHash<QString, bool> m_loggedKeys;

    // QHostInfo pending lookups: lookupId → remoteIp
    QHash<int, QString> m_pendingLookups;
    // For deferred domain writes: remoteIp → domain (filled by QHostInfo)
    QHash<QString, QString> m_resolvedDomains;
    QMutex m_resolvedMutex;
};

#endif // NETWORKLOGGER_H
