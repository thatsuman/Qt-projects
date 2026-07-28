#include "networkerrorreader.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace Analytics {

QList<ErrorRecord> NetworkErrorReader::readErrorLog(const QString &username)
{
    QList<ErrorRecord> records;

    const QString path = QString("logs/%1/network_error.txt").arg(username);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return records;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        ErrorRecord record;
        // Format: "2026-07-27T07:00:00.000Z - <message>"
        const int dashIdx = line.indexOf(QStringLiteral(" - "));
        if (dashIdx > 0) {
            const QString tsStr = line.left(dashIdx);
            record.timestamp = QDateTime::fromString(tsStr, Qt::ISODateWithMs);
            if (!record.timestamp.isValid()) {
                record.timestamp = QDateTime::fromString(tsStr, Qt::ISODate);
            }
            record.message = line.mid(dashIdx + 3);
        } else {
            record.message = line;
        }

        const QString lower = record.message.toLower();
        record.isDnsWarning    = lower.contains(QStringLiteral("dns")) ||
                                 lower.contains(QStringLiteral("unparsed"));
        record.isShutdownFlush = lower.contains(QStringLiteral("app_shutdown")) ||
                                 lower.contains(QStringLiteral("shutdown"));

        records.append(record);
    }

    return records;
}

QList<DiagRecord> NetworkErrorReader::readDiagLog(const QString &username)
{
    QList<DiagRecord> records;

    const QString path = QString("logs/%1/network_dns_diag.jsonl").arg(username);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return records; // file may not exist yet
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject()) continue;

        const QJsonObject obj = doc.object();
        DiagRecord record;
        record.eventId = obj.value(QStringLiteral("event_id")).toInt();

        const QString tsStr = obj.value(QStringLiteral("timestamp_utc")).toString();
        record.timestamp = QDateTime::fromString(tsStr, Qt::ISODate);

        const QJsonArray props = obj.value(QStringLiteral("property_names")).toArray();
        for (const QJsonValue &v : props) {
            record.propertyNames.append(v.toString());
        }

        records.append(record);
    }

    return records;
}

} // namespace Analytics
