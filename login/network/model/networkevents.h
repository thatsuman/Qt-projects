#ifndef NETWORK_MODEL_NETWORKEVENTS_H
#define NETWORK_MODEL_NETWORKEVENTS_H

#include "network/model/flowkey.h"

#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>

namespace Network {

struct TcpFlags
{
    bool syn = false;
    bool ack = false;
    bool fin = false;
    bool rst = false;
};

struct PacketObservation
{
    QDateTime timestampUtc;
    int ipVersion = 4;
    Direction direction = Direction::Unknown;
    TransportProtocol transport = TransportProtocol::Other;
    IpAddress srcIp;
    IpAddress dstIp;
    quint16 srcPort = 0;
    quint16 dstPort = 0;
    quint64 packetBytes = 0;
    quint64 payloadBytes = 0;
    QString visibleHostname;
    QString visibleAlpn;
    TcpFlags tcpFlags;
    bool loopback = false;
    quint32 interfaceIndex = 0;
};

enum class FlowLifecycleEvent {
    Established,
    Deleted
};

struct FlowLifecycleObservation
{
    QDateTime timestampUtc;
    FlowLifecycleEvent event = FlowLifecycleEvent::Established;
    quint32 pid = 0;
    FlowKey key;
    QString source = "windivert_flow";
};

struct ProcessConnectionSnapshot
{
    QDateTime timestampUtc;
    quint32 pid = 0;
    QString processName;
    QString processPath;
    QDateTime processCreationTimeUtc;
    FlowKey key;
    QString state;
    QString source = "iphelper";
};

struct DnsObservation
{
    QDateTime timestampUtc;
    QString queryName;
    QList<IpAddress> answerIps;
    int ttlSeconds = 60;
    quint32 pid = 0;
    QString processName;
    QString status;
    QString source = "etw_dns_client";
};

inline FlowKey normalizePacketKey(const PacketObservation &packet)
{
    FlowKey key;
    key.transport = packet.transport;
    key.ipVersion = packet.ipVersion;

    if (packet.direction == Direction::Inbound) {
        key.localIp = packet.dstIp;
        key.localPort = packet.dstPort;
        key.remoteIp = packet.srcIp;
        key.remotePort = packet.srcPort;
        return key;
    }

    key.localIp = packet.srcIp;
    key.localPort = packet.srcPort;
    key.remoteIp = packet.dstIp;
    key.remotePort = packet.dstPort;
    return key;
}

} // namespace Network

Q_DECLARE_METATYPE(Network::PacketObservation)
Q_DECLARE_METATYPE(Network::FlowLifecycleObservation)
Q_DECLARE_METATYPE(Network::ProcessConnectionSnapshot)
Q_DECLARE_METATYPE(Network::DnsObservation)

#endif // NETWORK_MODEL_NETWORKEVENTS_H
