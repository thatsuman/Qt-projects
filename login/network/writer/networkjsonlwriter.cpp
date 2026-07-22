#include "networkjsonlwriter.h"

#include <QDateTime>
#include <QDir>
#include <QJsonDocument>

namespace Network {

NetworkJsonlWriter::NetworkJsonlWriter(QObject *parent)
    : QObject(parent)
{
}

void NetworkJsonlWriter::start(const QString &username)
{
    stop();

    m_username = username;
    m_logDirectory = QString("logs/%1").arg(username);
    QDir().mkpath(m_logDirectory);

    m_sessionFile.setFileName(QString("%1/network_log.jsonl").arg(m_logDirectory));
    if (!m_sessionFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        logError(QString("Failed to open network_log.jsonl: %1").arg(m_sessionFile.errorString()));
    }

    m_errorFile.setFileName(QString("%1/network_error.txt").arg(m_logDirectory));
    if (!m_errorFile.open(QIODevice::Append | QIODevice::Text)) {
        return;
    }
}

void NetworkJsonlWriter::stop()
{
    if (m_sessionFile.isOpen()) {
        m_sessionFile.flush();
        m_sessionFile.close();
    }
    if (m_errorFile.isOpen()) {
        m_errorFile.flush();
        m_errorFile.close();
    }
    m_username.clear();
    m_logDirectory.clear();
}

void NetworkJsonlWriter::writeSession(const NetworkSessionRecord &record)
{
    if (!m_sessionFile.isOpen() || !shouldWriteSession(record)) {
        return;
    }

    const QByteArray line = QJsonDocument(record.toJson()).toJson(QJsonDocument::Compact);
    m_sessionFile.write(line);
    m_sessionFile.write("\n");
    m_sessionFile.flush();
}

void NetworkJsonlWriter::logError(const QString &message)
{
    if (!m_errorFile.isOpen() && !m_logDirectory.isEmpty()) {
        m_errorFile.setFileName(QString("%1/network_error.txt").arg(m_logDirectory));
        m_errorFile.open(QIODevice::Append | QIODevice::Text);
    }

    if (!m_errorFile.isOpen()) {
        return;
    }

    QTextStream out(&m_errorFile);
    out << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
        << " - " << message << "\n";
    m_errorFile.flush();
}

bool NetworkJsonlWriter::shouldWriteSession(const NetworkSessionRecord &record) const
{
    if (record.loopback || record.key.remoteIp.isNull()) {
        return false;
    }

    const quint64 totalBytes = record.bytesSentTotal + record.bytesReceivedTotal;
    const quint64 totalPackets = record.packetsSent + record.packetsReceived;
    const bool hasHostname = !record.remoteHost.primaryName.isEmpty();
    const bool hasProcess = record.process.pid != 0 || record.process.name != "unknown";

    if (!hasHostname && !hasProcess) {
        return false;
    }

    if (!hasHostname && totalBytes < 4096 && totalPackets < 8) {
        return false;
    }

    return true;
}

} // namespace Network
