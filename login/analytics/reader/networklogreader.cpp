#include "networklogreader.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace Analytics {

QList<NetworkRecord> NetworkLogReader::readLogs(const QString &username)
{
    QList<NetworkRecord> records;

    const QString path = QString("logs/%1/network_log.jsonl").arg(username);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return records;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject()) continue;

        const QJsonObject obj = doc.object();
        NetworkRecord record;

        // Timestamps (ISO format from network_log.jsonl)
        const QString startStr = obj.value(QStringLiteral("start_time_utc")).toString();
        const QString endStr   = obj.value(QStringLiteral("end_time_utc")).toString();
        record.startTime = QDateTime::fromString(startStr, Qt::ISODate).toUTC();
        record.endTime   = QDateTime::fromString(endStr,   Qt::ISODate).toUTC();

        // Remote endpoint
        const QJsonObject remote = obj.value(QStringLiteral("remote")).toObject();
        record.remoteIp          = remote.value(QStringLiteral("ip")).toString();
        record.remotePort        = remote.value(QStringLiteral("port")).toInt();
        record.remoteHostname    = remote.value(QStringLiteral("hostname")).toString();
        record.hostnameConfidence = remote.value(QStringLiteral("hostname_confidence")).toString();

        // Process attribution
        const QJsonObject process = obj.value(QStringLiteral("process")).toObject();
        record.processPid        = static_cast<quint32>(process.value(QStringLiteral("pid")).toInt());
        record.processName       = process.value(QStringLiteral("name")).toString();
        record.processConfidence = process.value(QStringLiteral("confidence")).toString();
        record.processSource     = process.value(QStringLiteral("source")).toString();

        // Protocol
        record.transportProtocol        = obj.value(QStringLiteral("transport_protocol")).toString();
        record.appProtocolHint          = obj.value(QStringLiteral("app_protocol_hint")).toString();
        record.applicationLayerCategory = obj.value(QStringLiteral("application_layer_category")).toString();

        // Traffic volumes
        const QJsonObject bytes   = obj.value(QStringLiteral("bytes")).toObject();
        record.bytesSent          = static_cast<quint64>(bytes.value(QStringLiteral("sent_total")).toDouble());
        record.bytesReceived      = static_cast<quint64>(bytes.value(QStringLiteral("received_total")).toDouble());

        const QJsonObject packets = obj.value(QStringLiteral("packets")).toObject();
        record.packetsSent        = static_cast<quint64>(packets.value(QStringLiteral("sent")).toDouble());
        record.packetsReceived    = static_cast<quint64>(packets.value(QStringLiteral("received")).toDouble());

        // Metadata
        record.closeReason        = obj.value(QStringLiteral("close_reason")).toString();
        record.mergedRecordCount  = obj.value(QStringLiteral("merged_record_count")).toInt(1);

        // Optional DNS sub-object (DNS resolver sessions only)
        const QJsonObject dns = obj.value(QStringLiteral("dns")).toObject();
        if (!dns.isEmpty()) {
            record.dnsQueryName = dns.value(QStringLiteral("query_name")).toString();
            const QJsonArray answerIps = dns.value(QStringLiteral("answer_ips")).toArray();
            for (const QJsonValue &ip : answerIps) {
                const QString ipStr = ip.toString();
                if (!ipStr.isEmpty()) record.dnsAnswerIps.append(ipStr);
            }
        }

        if (record.remoteIp.isEmpty()) continue; // skip malformed records

        records.append(record);
    }

    return records;
}

} // namespace Analytics
