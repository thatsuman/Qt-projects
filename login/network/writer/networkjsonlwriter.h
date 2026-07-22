#ifndef NETWORK_WRITER_NETWORKJSONLWRITER_H
#define NETWORK_WRITER_NETWORKJSONLWRITER_H

#include "network/model/networksessionrecord.h"

#include <QFile>
#include <QObject>
#include <QString>
#include <QTextStream>

namespace Network {

class NetworkJsonlWriter : public QObject
{
    Q_OBJECT

public:
    explicit NetworkJsonlWriter(QObject *parent = nullptr);

public slots:
    void start(const QString &username);
    void stop();
    void writeSession(const NetworkSessionRecord &record);
    void logError(const QString &message);

private:
    QString m_username;
    QString m_logDirectory;
    QFile m_sessionFile;
    QFile m_errorFile;
};

} // namespace Network

#endif // NETWORK_WRITER_NETWORKJSONLWRITER_H
