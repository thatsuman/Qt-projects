#ifndef NETWORK_PROTOCOL_PROTOCOLINFERENCER_H
#define NETWORK_PROTOCOL_PROTOCOLINFERENCER_H

#include "network/model/flowsession.h"

namespace Network {

class ProtocolInferencer
{
public:
    static ProtocolHint infer(TransportProtocol transport, quint16 localPort, quint16 remotePort);
};

} // namespace Network

#endif // NETWORK_PROTOCOL_PROTOCOLINFERENCER_H
