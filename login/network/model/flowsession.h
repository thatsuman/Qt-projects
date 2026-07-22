#ifndef NETWORK_MODEL_FLOWSESSION_H
#define NETWORK_MODEL_FLOWSESSION_H

#include "network/model/flowkey.h"

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QUuid>

namespace Network {

struct ProcessAttribution
{
    quint32 pid = 0;
    QString name = "unknown";
    QString path;
    QString source = "unknown";
    QString confidence = "none";
};

struct HostnameAttribution
{
    QString primaryName;
    QStringList candidates;
    QString source = "unknown";
    QString confidence = "none";
};

struct ProtocolHint
{
    QString hint = "unknown";
    QString confidence = "none";
    QString reason = "no_port_rule_matched";
};

struct FlowSession
{
    QString flowId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    FlowKey key;
    QDateTime startTimeUtc;
    QDateTime lastSeenUtc;
    Direction directionFirstSeen = Direction::Unknown;
    quint64 bytesSentTotal = 0;
    quint64 bytesReceivedTotal = 0;
    quint64 payloadBytesSent = 0;
    quint64 payloadBytesReceived = 0;
    quint64 packetsSent = 0;
    quint64 packetsReceived = 0;
    bool tcpSynSeen = false;
    bool tcpFinSeen = false;
    bool tcpRstSeen = false;
    QDateTime tcpFinSeenUtc;
    QString closeReason;
    ProcessAttribution process;
    HostnameAttribution remoteHost;
    ProtocolHint appProtocol;
    QString applicationLayerCategory = "unknown";
    bool ipv6 = false;
    bool loopback = false;
};

} // namespace Network

#endif // NETWORK_MODEL_FLOWSESSION_H
