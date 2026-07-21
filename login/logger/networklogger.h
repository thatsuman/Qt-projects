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
#include <windows.h>
#include <QHostInfo>

#include "../etw/EtwTraceSession.h"
#include "../network/ProtocolClassifier.h"

// ── Per-connection record stored in-memory per PID ───────────────────────────
struct ConnectionRecord {
    QString localIp;
    QString remoteIp;
    QList<int> localPorts;    // List of consolidated local ports
    QList<int> remotePorts;   // List of consolidated remote ports
    QString domain;           // resolved hostname
    QString protocol;         // e.g. "REST API", "WebSocket"
    
    quint64 bytesSent = 0;
    quint64 bytesReceived = 0;
    
    QDateTime firstSeen;
    QDateTime lastSeen;
    int connectionCount = 1;  // Number of consolidated flows
};

// ── Per-PID session tracking structure ───────────────────────────────────────
struct PidSession {
    DWORD            pid;
    QString          processName;
    QDateTime        firstSeen;
    QDateTime        lastSeen;
    QList<ConnectionRecord> connections;
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

    static QString formatBytes(quint64 bytes);

signals:
    void networkActivityOccurred(const QString &logText);

private slots:
    void onTimer();
    void onHostLookupDone(const QHostInfo &info);
    
    // ETW Event Slots
    void onEtwTcpEvent(const EtwTcpEvent &event);
    void onEtwHttpEvent(const EtwHttpEvent &event);

private:
    QString getProcessNameFromPid(DWORD pid);
    void flushPidSession(const PidSession &session);
    void flushAllSessions();

    QString logDir() const;          // returns "logs/<username>"
    QString networkLogPath() const;  // returns full path to network_log.jsonl

    QTimer  *m_timer;
    QString  m_currentUser;
    bool     m_active;
    EtwTraceSession *m_etwSession;
    ProtocolClassifier m_classifier;

    // PID → active session (connections still open)
    QHash<DWORD, PidSession> m_pidSessions;

    // QHostInfo pending lookups: lookupId → remoteIp
    QHash<int, QString> m_pendingLookups;
    // For deferred domain writes: remoteIp → domain (filled by QHostInfo)
    QHash<QString, QString> m_resolvedDomains;
    QMutex m_resolvedMutex;
};

#endif // NETWORKLOGGER_H
