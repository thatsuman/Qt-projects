#include "protocolinferencer.h"

namespace Network {

ProtocolHint ProtocolInferencer::infer(TransportProtocol transport, quint16 localPort, quint16 remotePort)
{
    const quint16 port = remotePort != 0 ? remotePort : localPort;
    ProtocolHint hint;

    if (transport == TransportProtocol::Tcp && port == 80) {
        hint.hint = "HTTP";
        hint.confidence = "medium";
        hint.reason = "remote_port_80_tcp";
    } else if (transport == TransportProtocol::Tcp && port == 443) {
        hint.hint = "HTTPS";
        hint.confidence = "medium";
        hint.reason = "remote_port_443_tcp";
    } else if (transport == TransportProtocol::Udp && port == 443) {
        hint.hint = "QUIC/HTTP3";
        hint.confidence = "medium-low";
        hint.reason = "remote_port_443_udp";
    } else if ((transport == TransportProtocol::Tcp || transport == TransportProtocol::Udp) && port == 53) {
        hint.hint = "DNS";
        hint.confidence = "high";
        hint.reason = "port_53";
    } else if (transport == TransportProtocol::Tcp && port == 853) {
        hint.hint = "DNS-over-TLS";
        hint.confidence = "medium";
        hint.reason = "remote_port_853_tcp";
    } else if (transport == TransportProtocol::Tcp && port == 22) {
        hint.hint = "SSH";
        hint.confidence = "medium";
        hint.reason = "remote_port_22_tcp";
    } else if (transport == TransportProtocol::Tcp && port == 25) {
        hint.hint = "SMTP";
        hint.confidence = "medium";
        hint.reason = "remote_port_25_tcp";
    } else if (transport == TransportProtocol::Tcp && port == 587) {
        hint.hint = "SMTP submission";
        hint.confidence = "medium";
        hint.reason = "remote_port_587_tcp";
    } else if (transport == TransportProtocol::Tcp && port == 993) {
        hint.hint = "IMAPS";
        hint.confidence = "medium";
        hint.reason = "remote_port_993_tcp";
    } else if (transport == TransportProtocol::Tcp && port == 995) {
        hint.hint = "POP3S";
        hint.confidence = "medium";
        hint.reason = "remote_port_995_tcp";
    } else {
        hint.hint = transportToString(transport);
        hint.confidence = "low";
        hint.reason = "transport_only";
    }

    return hint;
}

} // namespace Network
