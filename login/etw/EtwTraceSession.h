#ifndef ETWTRACESESSION_H
#define ETWTRACESESSION_H

#include <QThread>
#include <QString>
#include <QDateTime>
#include <QHash>
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>

struct EtwConnection {
    QString localIp;
    int localPort;
    QString remoteIp;
    int remotePort;
    DWORD pid;
};

struct EtwTcpEvent {
    QString localIp;
    int localPort;
    QString remoteIp;
    int remotePort;
    DWORD pid;
    quint64 bytesTransferred;
    bool isSend;
    QDateTime timestamp;
};

struct EtwHttpEvent {
    DWORD pid;
    QString url;
    QString method;
    int statusCode;
    QString contentType;
    QString protocol; // e.g. WebSocket, HTTP/1.1
    QDateTime timestamp;
};

class EtwTraceSession : public QThread
{
    Q_OBJECT
public:
    explicit EtwTraceSession(QObject *parent = nullptr);
    ~EtwTraceSession() override;

    bool startSession();
    void stopSession();

    static bool isUserAdminOrPerformanceLogUser();

signals:
    void tcpEventOccurred(const EtwTcpEvent &event);
    void httpEventOccurred(const EtwHttpEvent &event);

protected:
    void run() override;

private:
    static VOID WINAPI eventRecordCallback(PEVENT_RECORD pEventRecord);
    void handleEvent(PEVENT_RECORD pEventRecord);

    TRACEHANDLE m_traceHandle;
    TRACEHANDLE m_openedHandle;
    EVENT_TRACE_PROPERTIES *m_traceProperties;
    bool m_running;

    QHash<quint64, EtwConnection> m_tcpConnections;

    static EtwTraceSession *s_instance;
};

#endif // ETWTRACESESSION_H
