#ifndef NETWORK_MODEL_NETWORKSESSIONRECORD_H
#define NETWORK_MODEL_NETWORKSESSIONRECORD_H

#include "network/model/flowsession.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMetaType>

namespace Network {

struct NetworkSessionRecord
{
    QString username;
    QString flowId;
    QDateTime startTimeUtc;
    QDateTime endTimeUtc;
    FlowKey key;
    HostnameAttribution remoteHost;
    ProcessAttribution process;
    ProtocolHint appProtocol;
    quint64 bytesSentTotal = 0;
    quint64 bytesReceivedTotal = 0;
    quint64 payloadBytesSent = 0;
    quint64 payloadBytesReceived = 0;
    quint64 packetsSent = 0;
    quint64 packetsReceived = 0;
    QString closeReason;
    bool ipv6 = false;
    bool loopback = false;

    QJsonObject toJson() const
    {
        QJsonObject root;
        root["type"] = "network_session";
        root["schema_version"] = 1;
        root["username"] = username;
        root["flow_id"] = flowId;
        root["start_time_utc"] = startTimeUtc.toUTC().toString(Qt::ISODateWithMs);
        root["end_time_utc"] = endTimeUtc.toUTC().toString(Qt::ISODateWithMs);
        root["duration_ms"] = startTimeUtc.msecsTo(endTimeUtc);

        QJsonObject local;
        local["ip"] = key.localIp.toString();
        local["port"] = static_cast<int>(key.localPort);
        root["local"] = local;

        QJsonArray hostnameCandidates;
        for (const QString &candidate : remoteHost.candidates) {
            hostnameCandidates.append(candidate);
        }

        QJsonObject remote;
        remote["ip"] = key.remoteIp.toString();
        remote["port"] = static_cast<int>(key.remotePort);
        remote["hostname"] = remoteHost.primaryName.isEmpty() ? QJsonValue() : QJsonValue(remoteHost.primaryName);
        remote["hostname_source"] = remoteHost.source;
        remote["hostname_confidence"] = remoteHost.confidence;
        remote["hostname_candidates"] = hostnameCandidates;
        root["remote"] = remote;

        QJsonObject processObject;
        processObject["pid"] = static_cast<int>(process.pid);
        processObject["name"] = process.name;
        processObject["path"] = process.path;
        processObject["source"] = process.source;
        processObject["confidence"] = process.confidence;
        root["process"] = processObject;

        root["transport_protocol"] = transportToString(key.transport);
        root["app_protocol_hint"] = appProtocol.hint;
        root["app_protocol_confidence"] = appProtocol.confidence;
        root["app_protocol_reason"] = appProtocol.reason;

        QJsonObject bytes;
        bytes["sent_total"] = static_cast<double>(bytesSentTotal);
        bytes["received_total"] = static_cast<double>(bytesReceivedTotal);
        bytes["sent_payload"] = static_cast<double>(payloadBytesSent);
        bytes["received_payload"] = static_cast<double>(payloadBytesReceived);
        root["bytes"] = bytes;

        QJsonObject packets;
        packets["sent"] = static_cast<double>(packetsSent);
        packets["received"] = static_cast<double>(packetsReceived);
        root["packets"] = packets;

        root["close_reason"] = closeReason;

        QJsonObject flags;
        flags["ipv6"] = ipv6;
        flags["loopback"] = loopback;
        root["flags"] = flags;

        return root;
    }
};

} // namespace Network

Q_DECLARE_METATYPE(Network::NetworkSessionRecord)

#endif // NETWORK_MODEL_NETWORKSESSIONRECORD_H
