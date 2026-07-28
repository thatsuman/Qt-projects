#include "activitylogreader.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace Analytics {

QList<ActivityRecord> ActivityLogReader::readLogs(const QString &username)
{
    QList<ActivityRecord> records;

    const QString path = QString("logs/%1/activity_log.jsonl").arg(username);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return records; // not an error — log may not exist yet
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject()) continue;

        const QJsonObject obj = doc.object();
        const QString type = obj.value(QStringLiteral("type")).toString();

        // When a new session_start marker is encountered, clear previous records
        // to isolate activity data to the latest active login session only.
        if (type == QStringLiteral("session_start")) {
            records.clear();
            continue;
        }

        // Only process entries with type "activity"
        if (type != QStringLiteral("activity")) {
            continue;
        }

        ActivityRecord record;

        // Parse timestamps — activity_log uses local time format "yyyy-MM-dd hh:mm:ss"
        const QString startStr = obj.value(QStringLiteral("timestamp_start")).toString();
        const QString endStr   = obj.value(QStringLiteral("timestamp_end")).toString();
        record.startTime = QDateTime::fromString(startStr, QStringLiteral("yyyy-MM-dd hh:mm:ss"));
        record.endTime   = QDateTime::fromString(endStr,   QStringLiteral("yyyy-MM-dd hh:mm:ss"));

        record.windowTitle   = obj.value(QStringLiteral("window_title")).toString();
        record.processName   = obj.value(QStringLiteral("process_name")).toString();
        record.mouseDistancePx = obj.value(QStringLiteral("mouse_distance_px")).toDouble();

        // PRIVACY: read keystroke string, take length only, discard string immediately
        const QString keystrokes = obj.value(QStringLiteral("keystrokes")).toString();
        record.keystrokeCount = keystrokes.length();
        // keystrokes goes out of scope here — string is not stored anywhere

        // Skip records without valid process names
        if (record.processName.isEmpty()) continue;

        records.append(record);
    }

    return records;
}

} // namespace Analytics
