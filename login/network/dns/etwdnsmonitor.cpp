#include "etwdnsmonitor.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QVector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <evntcons.h>
#include <tdh.h>
#include <ws2tcpip.h>

namespace Network {

namespace {

const GUID DnsClientProviderGuid =
{ 0x1C95126E, 0x7EEA, 0x49A9, { 0xA3, 0xFE, 0xA3, 0x78, 0xB0, 0x3D, 0xDB, 0x4D } };

QDateTime fileTimeToDateTimeUtc(ULONGLONG value)
{
    if (value == 0) {
        return QDateTime::currentDateTimeUtc();
    }

    const quint64 msecsSince1601 = value / 10000ULL;
    const quint64 epochDiffMsecs = 11644473600000ULL;
    if (msecsSince1601 < epochDiffMsecs) {
        return QDateTime::currentDateTimeUtc();
    }
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(msecsSince1601 - epochDiffMsecs), Qt::UTC);
}

QString propertyName(PTRACE_EVENT_INFO info, const EVENT_PROPERTY_INFO &property)
{
    if (property.NameOffset == 0) {
        return QString();
    }
    auto name = reinterpret_cast<const wchar_t *>(reinterpret_cast<const BYTE *>(info) + property.NameOffset);
    return QString::fromWCharArray(name);
}

QString propertyToString(EVENT_RECORD *record, PTRACE_EVENT_INFO info, const EVENT_PROPERTY_INFO &property)
{
    if (property.NameOffset == 0) {
        return QString();
    }

    PROPERTY_DATA_DESCRIPTOR descriptor = {};
    descriptor.PropertyName = reinterpret_cast<ULONGLONG>(reinterpret_cast<const BYTE *>(info) + property.NameOffset);
    descriptor.ArrayIndex = ULONG_MAX;

    ULONG size = 0;
    if (TdhGetPropertySize(record, 0, nullptr, 1, &descriptor, &size) != ERROR_SUCCESS || size == 0) {
        return QString();
    }

    QByteArray buffer(static_cast<int>(size), 0);
    if (TdhGetProperty(record, 0, nullptr, 1, &descriptor, size,
                       reinterpret_cast<PBYTE>(buffer.data())) != ERROR_SUCCESS) {
        return QString();
    }

    const USHORT inType = property.nonStructType.InType;
    if (inType == TDH_INTYPE_UNICODESTRING) {
        return QString::fromWCharArray(reinterpret_cast<const wchar_t *>(buffer.constData()));
    }
    if (inType == TDH_INTYPE_ANSISTRING) {
        return QString::fromLocal8Bit(buffer.constData());
    }
    if (inType == TDH_INTYPE_UINT32 && size >= sizeof(UINT32)) {
        return QString::number(*reinterpret_cast<const UINT32 *>(buffer.constData()));
    }
    if (inType == TDH_INTYPE_INT32 && size >= sizeof(INT32)) {
        return QString::number(*reinterpret_cast<const INT32 *>(buffer.constData()));
    }

    return QString();
}

QList<IpAddress> extractIpAddresses(const QString &value)
{
    QList<IpAddress> result;
    static const QRegularExpression tokenRegex(QStringLiteral("[0-9A-Fa-f:.]+"));
    QRegularExpressionMatchIterator it = tokenRegex.globalMatch(value);
    while (it.hasNext()) {
        const QString token = it.next().captured(0);
        if (token.contains('.')) {
            const QStringList octets = token.split('.');
            if (octets.size() == 4) {
                bool ok = true;
                quint8 bytes[4] = {};
                for (int i = 0; i < 4; ++i) {
                    const int octet = octets.at(i).toInt(&ok);
                    if (!ok || octet < 0 || octet > 255) {
                        ok = false;
                        break;
                    }
                    bytes[i] = static_cast<quint8>(octet);
                }
                if (ok) {
                    result.append(IpAddress::fromV4Bytes(bytes[0], bytes[1], bytes[2], bytes[3]));
                }
            }
        } else if (token.contains(':')) {
            IN6_ADDR addr = {};
            if (InetPtonW(AF_INET6, reinterpret_cast<PCWSTR>(token.utf16()), &addr) == 1) {
                result.append(IpAddress::fromV6(addr.u.Byte));
            }
        }
    }
    return result;
}

} // namespace

EtwDnsMonitor::EtwDnsMonitor(QObject *parent)
    : QObject(parent)
{
}

void EtwDnsMonitor::start()
{
    if (m_running.exchange(true)) {
        return;
    }
    m_stopRequested.store(false);

    if (QProcessEnvironment::systemEnvironment().contains("LOGIN_FORCE_ETW_FAIL")) {
        m_running.store(false);
        emit errorOccurred("ETW DNS monitor forced to fail by LOGIN_FORCE_ETW_FAIL.");
        emit statusChanged("DNS correlation degraded");
        return;
    }

    m_sessionName = QString("LoginDnsTrace-%1").arg(QCoreApplication::applicationPid());

    const qsizetype propertyBytes = sizeof(EVENT_TRACE_PROPERTIES) + (m_sessionName.size() + 1) * sizeof(wchar_t);
    QVector<quint8> propertyBuffer(static_cast<int>(propertyBytes));
    auto *properties = reinterpret_cast<EVENT_TRACE_PROPERTIES *>(propertyBuffer.data());
    properties->Wnode.BufferSize = static_cast<ULONG>(propertyBuffer.size());
    properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    properties->FlushTimer = 1;

    const ULONG startStatus = StartTraceW(&m_sessionHandle,
                                          reinterpret_cast<LPCWSTR>(m_sessionName.utf16()),
                                          properties);
    if (startStatus == ERROR_ALREADY_EXISTS) {
        ControlTraceW(0, reinterpret_cast<LPCWSTR>(m_sessionName.utf16()), properties, EVENT_TRACE_CONTROL_STOP);
    } else if (startStatus != ERROR_SUCCESS) {
        m_running.store(false);
        emit errorOccurred(QString("ETW DNS StartTrace failed: %1").arg(startStatus));
        emit statusChanged("DNS correlation degraded");
        return;
    }

    if (startStatus == ERROR_ALREADY_EXISTS) {
        const ULONG retryStatus = StartTraceW(&m_sessionHandle,
                                              reinterpret_cast<LPCWSTR>(m_sessionName.utf16()),
                                              properties);
        if (retryStatus != ERROR_SUCCESS) {
            m_running.store(false);
            emit errorOccurred(QString("ETW DNS StartTrace retry failed: %1").arg(retryStatus));
            emit statusChanged("DNS correlation degraded");
            return;
        }
    }

    const ULONG enableStatus = EnableTraceEx2(m_sessionHandle,
                                              &DnsClientProviderGuid,
                                              EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                                              TRACE_LEVEL_INFORMATION,
                                              0, 0, 0, nullptr);
    if (enableStatus != ERROR_SUCCESS) {
        emit errorOccurred(QString("ETW DNS EnableTraceEx2 failed: %1").arg(enableStatus));
        stopSession();
        m_running.store(false);
        emit statusChanged("DNS correlation degraded");
        return;
    }

    EVENT_TRACE_LOGFILEW logfile = {};
    logfile.LoggerName = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(m_sessionName.utf16()));
    logfile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    logfile.EventRecordCallback = &EtwDnsMonitor::eventRecordCallback;
    logfile.Context = this;

    m_traceHandle = OpenTraceW(&logfile);
    if (m_traceHandle == INVALID_PROCESSTRACE_HANDLE) {
        emit errorOccurred(QString("ETW DNS OpenTrace failed: %1").arg(GetLastError()));
        stopSession();
        m_running.store(false);
        emit statusChanged("DNS correlation degraded");
        return;
    }

    emit statusChanged("DNS correlation active");
    const ULONG processStatus = ProcessTrace(&m_traceHandle, 1, nullptr, nullptr);
    if (processStatus != ERROR_SUCCESS && processStatus != ERROR_CANCELLED && !m_stopRequested.load()) {
        emit errorOccurred(QString("ETW DNS ProcessTrace failed: %1").arg(processStatus));
    }

    stopSession();
    m_running.store(false);
    emit statusChanged("DNS correlation stopped");
}

void EtwDnsMonitor::stop()
{
    m_stopRequested.store(true);
    stopSession();
}

void WINAPI EtwDnsMonitor::eventRecordCallback(EVENT_RECORD *record)
{
    auto *self = reinterpret_cast<EtwDnsMonitor *>(record->UserContext);
    if (self) {
        self->handleEventRecord(record);
    }
}

void EtwDnsMonitor::handleEventRecord(EVENT_RECORD *record)
{
    ULONG infoSize = 0;
    ULONG status = TdhGetEventInformation(record, 0, nullptr, nullptr, &infoSize);
    if (status != ERROR_INSUFFICIENT_BUFFER || infoSize == 0) {
        return;
    }

    QByteArray infoBuffer(static_cast<int>(infoSize), 0);
    auto *info = reinterpret_cast<PTRACE_EVENT_INFO>(infoBuffer.data());
    status = TdhGetEventInformation(record, 0, nullptr, info, &infoSize);
    if (status != ERROR_SUCCESS) {
        return;
    }

    DnsObservation observation;
    observation.timestampUtc = fileTimeToDateTimeUtc(record->EventHeader.TimeStamp.QuadPart);
    observation.pid = record->EventHeader.ProcessId;
    observation.ttlSeconds = 60;
    observation.source = "etw_dns_client";

    QStringList propertyNames;
    for (ULONG i = 0; i < info->TopLevelPropertyCount; ++i) {
        const EVENT_PROPERTY_INFO &property = info->EventPropertyInfoArray[i];
        const QString name = propertyName(info, property);
        if (name.isEmpty()) {
            continue;
        }
        propertyNames.append(name);

        const QString value = propertyToString(record, info, property).trimmed();
        if (value.isEmpty()) {
            continue;
        }

        const QString lowerName = name.toLower();
        if ((lowerName.contains("query") || lowerName.contains("name") || lowerName.contains("host"))
                && observation.queryName.isEmpty()
                && !value.contains(' ')
                && !value.contains('\\')) {
            observation.queryName = value;
        }

        if (lowerName.contains("pid") || lowerName.contains("processid")) {
            bool ok = false;
            const quint32 pid = value.toUInt(&ok);
            if (ok && pid != 0) {
                observation.pid = pid;
            }
        }

        if (lowerName.contains("ttl")) {
            bool ok = false;
            const int ttl = value.toInt(&ok);
            if (ok && ttl > 0) {
                observation.ttlSeconds = ttl;
            }
        }

        if (lowerName.contains("addr") || lowerName.contains("ip") || lowerName.contains("answer")) {
            const QList<IpAddress> ips = extractIpAddresses(value);
            for (const IpAddress &ip : ips) {
                if (!observation.answerIps.contains(ip)) {
                    observation.answerIps.append(ip);
                }
            }
        }
    }

    if (!observation.queryName.isEmpty() && !observation.answerIps.isEmpty()) {
        observation.status = "success";
        emit dnsObserved(observation);
    } else {
        logUnknownEvent(record, propertyNames);
    }
}

void EtwDnsMonitor::logUnknownEvent(EVENT_RECORD *record, const QStringList &propertyNames)
{
    emit errorOccurred(QString("Unparsed DNS ETW event id=%1 properties=[%2]")
                       .arg(record->EventHeader.EventDescriptor.Id)
                       .arg(propertyNames.join(", ")));
}

void EtwDnsMonitor::stopSession()
{
    if (m_traceHandle && m_traceHandle != INVALID_PROCESSTRACE_HANDLE) {
        CloseTrace(m_traceHandle);
        m_traceHandle = 0;
    }

    if (m_sessionHandle != 0 || !m_sessionName.isEmpty()) {
        const qsizetype propertyBytes = sizeof(EVENT_TRACE_PROPERTIES) + (m_sessionName.size() + 1) * sizeof(wchar_t);
        QVector<quint8> propertyBuffer(static_cast<int>(propertyBytes));
        auto *properties = reinterpret_cast<EVENT_TRACE_PROPERTIES *>(propertyBuffer.data());
        properties->Wnode.BufferSize = static_cast<ULONG>(propertyBuffer.size());
        properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
        ControlTraceW(m_sessionHandle,
                      reinterpret_cast<LPCWSTR>(m_sessionName.utf16()),
                      properties,
                      EVENT_TRACE_CONTROL_STOP);
        m_sessionHandle = 0;
    }
}

} // namespace Network
