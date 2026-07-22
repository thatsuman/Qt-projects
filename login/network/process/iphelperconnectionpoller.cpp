#include "iphelperconnectionpoller.h"

#include <QByteArray>
#include <QProcessEnvironment>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

namespace Network {

namespace {

IpAddress ipFromNetworkOrder(DWORD address)
{
    const quint32 host = ntohl(address);
    return IpAddress::fromV4(host);
}

QString tcpStateToString(DWORD state)
{
    switch (state) {
    case MIB_TCP_STATE_CLOSED: return "closed";
    case MIB_TCP_STATE_LISTEN: return "listen";
    case MIB_TCP_STATE_SYN_SENT: return "syn_sent";
    case MIB_TCP_STATE_SYN_RCVD: return "syn_received";
    case MIB_TCP_STATE_ESTAB: return "established";
    case MIB_TCP_STATE_FIN_WAIT1: return "fin_wait1";
    case MIB_TCP_STATE_FIN_WAIT2: return "fin_wait2";
    case MIB_TCP_STATE_CLOSE_WAIT: return "close_wait";
    case MIB_TCP_STATE_CLOSING: return "closing";
    case MIB_TCP_STATE_LAST_ACK: return "last_ack";
    case MIB_TCP_STATE_TIME_WAIT: return "time_wait";
    case MIB_TCP_STATE_DELETE_TCB: return "delete_tcb";
    default: return "unknown";
    }
}

} // namespace

IpHelperConnectionPoller::IpHelperConnectionPoller(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &IpHelperConnectionPoller::poll);
}

void IpHelperConnectionPoller::start()
{
    if (QProcessEnvironment::systemEnvironment().contains("LOGIN_FORCE_IPHELPER_FAIL")) {
        emit errorOccurred("IP Helper poller forced to fail by LOGIN_FORCE_IPHELPER_FAIL.");
        return;
    }

    poll();
    m_timer->start();
}

void IpHelperConnectionPoller::stop()
{
    m_timer->stop();
}

void IpHelperConnectionPoller::poll()
{
    if (QProcessEnvironment::systemEnvironment().contains("LOGIN_FORCE_IPHELPER_FAIL")) {
        emit errorOccurred("IP Helper poller forced to fail by LOGIN_FORCE_IPHELPER_FAIL.");
        return;
    }

    pollTcp();
    pollUdp();
}

void IpHelperConnectionPoller::pollTcp()
{
    DWORD size = 0;
    DWORD result = GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (result != ERROR_INSUFFICIENT_BUFFER) {
        emit errorOccurred(QString("GetExtendedTcpTable size query failed: %1").arg(result));
        return;
    }

    QByteArray buffer(static_cast<int>(size), 0);
    auto *table = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());
    result = GetExtendedTcpTable(table, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (result != NO_ERROR) {
        emit errorOccurred(QString("GetExtendedTcpTable failed: %1").arg(result));
        return;
    }

    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const MIB_TCPROW_OWNER_PID &row = table->table[i];
        ProcessInfo process = m_processResolver.resolve(row.dwOwningPid);

        ProcessConnectionSnapshot snapshot;
        snapshot.timestampUtc = nowUtc;
        snapshot.pid = row.dwOwningPid;
        snapshot.processName = process.name;
        snapshot.processPath = process.path;
        snapshot.processCreationTimeUtc = process.creationTimeUtc;
        snapshot.key.localIp = ipFromNetworkOrder(row.dwLocalAddr);
        snapshot.key.localPort = ntohs(static_cast<u_short>(row.dwLocalPort));
        snapshot.key.remoteIp = ipFromNetworkOrder(row.dwRemoteAddr);
        snapshot.key.remotePort = ntohs(static_cast<u_short>(row.dwRemotePort));
        snapshot.key.transport = TransportProtocol::Tcp;
        snapshot.key.ipVersion = 4;
        snapshot.state = tcpStateToString(row.dwState);

        emit processSnapshotObserved(snapshot);
    }
}

void IpHelperConnectionPoller::pollUdp()
{
    DWORD size = 0;
    DWORD result = GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    if (result != ERROR_INSUFFICIENT_BUFFER) {
        emit errorOccurred(QString("GetExtendedUdpTable size query failed: %1").arg(result));
        return;
    }

    QByteArray buffer(static_cast<int>(size), 0);
    auto *table = reinterpret_cast<PMIB_UDPTABLE_OWNER_PID>(buffer.data());
    result = GetExtendedUdpTable(table, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    if (result != NO_ERROR) {
        emit errorOccurred(QString("GetExtendedUdpTable failed: %1").arg(result));
        return;
    }

    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const MIB_UDPROW_OWNER_PID &row = table->table[i];
        ProcessInfo process = m_processResolver.resolve(row.dwOwningPid);

        ProcessConnectionSnapshot snapshot;
        snapshot.timestampUtc = nowUtc;
        snapshot.pid = row.dwOwningPid;
        snapshot.processName = process.name;
        snapshot.processPath = process.path;
        snapshot.processCreationTimeUtc = process.creationTimeUtc;
        snapshot.key.localIp = ipFromNetworkOrder(row.dwLocalAddr);
        snapshot.key.localPort = ntohs(static_cast<u_short>(row.dwLocalPort));
        snapshot.key.transport = TransportProtocol::Udp;
        snapshot.key.ipVersion = 4;
        snapshot.state = "listening";

        emit processSnapshotObserved(snapshot);
    }
}

} // namespace Network
