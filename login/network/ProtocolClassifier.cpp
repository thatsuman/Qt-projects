#include "ProtocolClassifier.h"

QString protocolToString(NetworkProtocol proto)
{
    switch (proto) {
        case NetworkProtocol::REST_API:  return "REST API";
        case NetworkProtocol::WEBSOCKET: return "WebSocket";
        case NetworkProtocol::HTTP1_1:   return "HTTP/1.1";
        case NetworkProtocol::HTTP2:     return "HTTP/2";
        case NetworkProtocol::GRPC:      return "gRPC";
        case NetworkProtocol::TLS:       return "TLS/SSL";
        case NetworkProtocol::DNS:       return "DNS";
        case NetworkProtocol::RAW_TCP:   return "TCP";
        case NetworkProtocol::RAW_UDP:   return "UDP";
    }
    return "UNKNOWN";
}

ProtocolClassifier::ProtocolClassifier()
{
}

NetworkProtocol ProtocolClassifier::classify(const ConnectionTuple &tuple, 
                                             const QString &url, 
                                             const QString &method, 
                                             const QString &contentType)
{
    // If we already have a high-level protocol classification, respect it
    NetworkProtocol current = getActiveProtocol(tuple);
    if (current == NetworkProtocol::WEBSOCKET || current == NetworkProtocol::GRPC) {
        return current;
    }

    // 1. gRPC Detection
    if (contentType.compare("application/grpc", Qt::CaseInsensitive) == 0) {
        recordTransition(tuple, NetworkProtocol::GRPC);
        return NetworkProtocol::GRPC;
    }

    // 2. WebSocket Detection
    if (!url.isEmpty() && (url.startsWith("ws://", Qt::CaseInsensitive) || url.startsWith("wss://", Qt::CaseInsensitive))) {
        recordTransition(tuple, NetworkProtocol::WEBSOCKET);
        return NetworkProtocol::WEBSOCKET;
    }

    // 3. REST API Heuristics
    if (!url.isEmpty()) {
        bool isRestMethod = (method == "GET" || method == "POST" || method == "PUT" || method == "DELETE" || method == "PATCH");
        bool isRestContent = (contentType.contains("json", Qt::CaseInsensitive) || 
                              contentType.contains("xml", Qt::CaseInsensitive) ||
                              contentType.contains("urlencoded", Qt::CaseInsensitive));
        bool isRestUrl = url.contains("/api/", Qt::CaseInsensitive) || url.contains("/rest/", Qt::CaseInsensitive);

        if (isRestMethod && (isRestContent || isRestUrl)) {
            recordTransition(tuple, NetworkProtocol::REST_API);
            return NetworkProtocol::REST_API;
        }
    }

    // 4. DNS Detection
    if (tuple.remotePort == 53 || tuple.localPort == 53) {
        recordTransition(tuple, NetworkProtocol::DNS);
        return NetworkProtocol::DNS;
    }

    // 5. TLS Detection
    if (tuple.remotePort == 443 || tuple.localPort == 443) {
        if (current == NetworkProtocol::RAW_TCP) {
            recordTransition(tuple, NetworkProtocol::TLS);
            return NetworkProtocol::TLS;
        }
    }

    // If current is not set, set to default raw TCP
    if (current == NetworkProtocol::RAW_TCP) {
        return NetworkProtocol::RAW_TCP;
    }

    // Fallback to RAW_TCP
    recordTransition(tuple, NetworkProtocol::RAW_TCP);
    return NetworkProtocol::RAW_TCP;
}

void ProtocolClassifier::recordTransition(const ConnectionTuple &tuple, NetworkProtocol proto)
{
    m_activeConnections[tuple] = proto;
}

NetworkProtocol ProtocolClassifier::getActiveProtocol(const ConnectionTuple &tuple) const
{
    return m_activeConnections.value(tuple, NetworkProtocol::RAW_TCP);
}

void ProtocolClassifier::removeConnection(const ConnectionTuple &tuple)
{
    m_activeConnections.remove(tuple);
}
