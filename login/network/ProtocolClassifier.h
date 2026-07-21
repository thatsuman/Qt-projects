#ifndef PROTOCOLCLASSIFIER_H
#define PROTOCOLCLASSIFIER_H

#include <QString>
#include <QHash>
#include <QDateTime>
#include <windows.h>

enum class NetworkProtocol {
    REST_API,
    WEBSOCKET,
    HTTP1_1,
    HTTP2,
    GRPC,
    TLS,
    DNS,
    RAW_TCP,
    RAW_UDP
};

QString protocolToString(NetworkProtocol proto);

struct ConnectionTuple {
    QString localIp;
    int localPort;
    QString remoteIp;
    int remotePort;
    DWORD pid;

    bool operator==(const ConnectionTuple &other) const {
        return localIp == other.localIp &&
               localPort == other.localPort &&
               remoteIp == other.remoteIp &&
               remotePort == other.remotePort &&
               pid == other.pid;
    }
};

inline uint qHash(const ConnectionTuple &key, uint seed = 0) {
    return qHash(key.localIp, seed) ^ qHash(key.localPort, seed) ^ 
           qHash(key.remoteIp, seed) ^ qHash(key.remotePort, seed) ^ qHash(key.pid, seed);
}

class ProtocolClassifier
{
public:
    ProtocolClassifier();

    NetworkProtocol classify(const ConnectionTuple &tuple, 
                             const QString &url = "", 
                             const QString &method = "", 
                             const QString &contentType = "");

    void recordTransition(const ConnectionTuple &tuple, NetworkProtocol proto);
    NetworkProtocol getActiveProtocol(const ConnectionTuple &tuple) const;
    void removeConnection(const ConnectionTuple &tuple);

private:
    QHash<ConnectionTuple, NetworkProtocol> m_activeConnections;
};

#endif // PROTOCOLCLASSIFIER_H
