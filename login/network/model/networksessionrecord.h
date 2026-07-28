#ifndef NETWORK_MODEL_NETWORKSESSIONRECORD_H
#define NETWORK_MODEL_NETWORKSESSIONRECORD_H

#include "network/model/flowsession.h"

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
    QString applicationLayerCategory = "unknown";
    quint64 bytesSentTotal = 0;
    quint64 bytesReceivedTotal = 0;
    quint64 payloadBytesSent = 0;
    quint64 payloadBytesReceived = 0;
    quint64 packetsSent = 0;
    quint64 packetsReceived = 0;
    QString closeReason;
    bool ipv6 = false;
    bool loopback = false;
    int mergedRecordCount = 1;
    bool isMergedConsecutiveRun = false;

    QJsonObject toJson() const
    {
        QJsonObject root;

        QJsonObject processObject;
        processObject["pid"] = static_cast<int>(process.pid);
        processObject["name"] = process.name;
        processObject["confidence"] = process.confidence;
        root["process"] = processObject;

        QJsonObject remote;
        remote["ip"] = key.remoteIp.toString();
        remote["port"] = static_cast<int>(key.remotePort);
        remote["hostname"] = remoteHost.primaryName.isEmpty() ? QJsonValue() : QJsonValue(remoteHost.primaryName);
        remote["hostname_confidence"] = remoteHost.confidence;
        remote["hostname_status"] = remoteHost.status;
        remote["hostname_reason"] = remoteHost.reason;
        root["remote"] = remote;

        root["transport_protocol"] = transportToString(key.transport);
        root["app_protocol_hint"] = appProtocol.hint;
        root["application_layer_category"] = applicationLayerCategory;

        QJsonObject bytes;
        bytes["sent_total"] = static_cast<double>(bytesSentTotal);
        bytes["received_total"] = static_cast<double>(bytesReceivedTotal);
        root["bytes"] = bytes;

        QJsonObject packets;
        packets["sent"] = static_cast<double>(packetsSent);
        packets["received"] = static_cast<double>(packetsReceived);
        root["packets"] = packets;

        root["start_time_utc"] = startTimeUtc.toUTC().toString(Qt::ISODate);
        root["end_time_utc"] = endTimeUtc.toUTC().toString(Qt::ISODate);
        root["close_reason"] = closeReason;
        root["merged_record_count"] = mergedRecordCount;
        root["is_merged_consecutive_run"] = (mergedRecordCount > 1 || isMergedConsecutiveRun);
        root["schema_version"] = 1;

        return root;
    }
};
} // namespace Network

Q_DECLARE_METATYPE(Network::NetworkSessionRecord)

#endif // NETWORK_MODEL_NETWORKSESSIONRECORD_H
