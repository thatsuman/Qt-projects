#ifndef NETWORK_DNS_ETWDNSMONITOR_H
#define NETWORK_DNS_ETWDNSMONITOR_H

#include "network/model/networkevents.h"

#include <QObject>
#include <QStringList>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>

#include <atomic>

namespace Network {

class EtwDnsMonitor : public QObject
{
    Q_OBJECT

public:
    explicit EtwDnsMonitor(QObject *parent = nullptr);

signals:
    void dnsObserved(const DnsObservation &observation);
    void errorOccurred(const QString &message);
    void statusChanged(const QString &status);

public slots:
    void start();
    void stop();

private:
    static void WINAPI eventRecordCallback(EVENT_RECORD *record);

    void handleEventRecord(EVENT_RECORD *record);
    void logUnknownEvent(EVENT_RECORD *record, const QStringList &propertyNames);
    void stopSession();

    std::atomic_bool m_running{false};
    std::atomic_bool m_stopRequested{false};
    TRACEHANDLE m_sessionHandle = 0;
    TRACEHANDLE m_traceHandle = 0;
    QString m_sessionName;
};

} // namespace Network

#endif // NETWORK_DNS_ETWDNSMONITOR_H
