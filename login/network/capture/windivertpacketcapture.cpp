#include "windivertpacketcapture.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QProcessEnvironment>
#include <QString>

#include "windivert.h"

namespace Network {

namespace {

using WinDivertOpenFn = HANDLE (WINAPI *)(const char *, WINDIVERT_LAYER, INT16, UINT64);
using WinDivertRecvFn = BOOL (WINAPI *)(HANDLE, VOID *, UINT, UINT *, WINDIVERT_ADDRESS *);
using WinDivertHelperParsePacketFn = BOOL (WINAPI *)(const VOID *, UINT, PWINDIVERT_IPHDR *,
                                                     PWINDIVERT_IPV6HDR *, UINT8 *, PWINDIVERT_ICMPHDR *,
                                                     PWINDIVERT_ICMPV6HDR *, PWINDIVERT_TCPHDR *,
                                                     PWINDIVERT_UDPHDR *, PVOID *, UINT *, PVOID *, UINT *);
using WinDivertHelperNtohsFn = UINT16 (WINAPI *)(UINT16);
using WinDivertHelperNtohlFn = UINT32 (WINAPI *)(UINT32);

HMODULE loadWinDivertLibrary()
{
    HMODULE library = LoadLibraryA("WinDivert.dll");
    if (library) {
        return library;
    }

    const QString appPath = QDir(QCoreApplication::applicationDirPath()).filePath("WinDivert.dll");
    library = LoadLibraryW(reinterpret_cast<LPCWSTR>(appPath.utf16()));
    if (library) {
        return library;
    }

    const QString vendoredPath = QDir(QCoreApplication::applicationDirPath())
            .filePath("../third_party/WinDivert/x64/WinDivert.dll");
    library = LoadLibraryW(reinterpret_cast<LPCWSTR>(QDir::cleanPath(vendoredPath).utf16()));
    if (library) {
        return library;
    }

    const QString sourceTreePath = QDir::current().filePath("third_party/WinDivert/x64/WinDivert.dll");
    return LoadLibraryW(reinterpret_cast<LPCWSTR>(QDir::cleanPath(sourceTreePath).utf16()));
}

TransportProtocol transportFromProtocol(UINT8 protocol)
{
    if (protocol == 6) {
        return TransportProtocol::Tcp;
    }
    if (protocol == 17) {
        return TransportProtocol::Udp;
    }
    if (protocol == 1 || protocol == 58) {
        return TransportProtocol::Icmp;
    }
    return TransportProtocol::Other;
}

IpAddress fromV6Words(const UINT32 *words)
{
    return IpAddress::fromV6(reinterpret_cast<const quint8 *>(words));
}

quint16 readBigEndian16(const quint8 *data)
{
    return static_cast<quint16>((static_cast<quint16>(data[0]) << 8) | data[1]);
}

quint32 readBigEndian24(const quint8 *data)
{
    return (static_cast<quint32>(data[0]) << 16)
         | (static_cast<quint32>(data[1]) << 8)
         | static_cast<quint32>(data[2]);
}

bool isReasonableHostname(const QString &hostname)
{
    if (hostname.size() < 3 || hostname.size() > 253 || !hostname.contains('.')) {
        return false;
    }

    for (const QChar ch : hostname) {
        if (ch.isLetterOrNumber() || ch == '-' || ch == '.') {
            continue;
        }
        return false;
    }
    return true;
}

struct TlsClientHelloMetadata
{
    QString serverName;
    QString alpn;
};

TlsClientHelloMetadata extractTlsClientHelloMetadata(const void *payload, UINT payloadLen)
{
    TlsClientHelloMetadata metadata;
    if (!payload || payloadLen < 43) {
        return metadata;
    }

    const auto *data = reinterpret_cast<const quint8 *>(payload);
    if (data[0] != 0x16 || data[5] != 0x01) {
        return metadata;
    }

    const quint16 recordLength = readBigEndian16(data + 3);
    if (static_cast<UINT>(recordLength + 5) > payloadLen) {
        return metadata;
    }

    const quint32 handshakeLength = readBigEndian24(data + 6);
    const UINT handshakeEnd = 9U + handshakeLength;
    if (handshakeEnd > payloadLen) {
        return metadata;
    }

    UINT offset = 9;
    offset += 2;  // client_version
    offset += 32; // random
    if (offset + 1 > handshakeEnd) {
        return metadata;
    }

    const UINT sessionIdLength = data[offset++];
    offset += sessionIdLength;
    if (offset + 2 > handshakeEnd) {
        return metadata;
    }

    const UINT cipherSuitesLength = readBigEndian16(data + offset);
    offset += 2 + cipherSuitesLength;
    if (offset + 1 > handshakeEnd) {
        return metadata;
    }

    const UINT compressionMethodsLength = data[offset++];
    offset += compressionMethodsLength;
    if (offset + 2 > handshakeEnd) {
        return metadata;
    }

    const UINT extensionsLength = readBigEndian16(data + offset);
    offset += 2;
    const UINT extensionsEnd = offset + extensionsLength;
    if (extensionsEnd > handshakeEnd) {
        return metadata;
    }

    while (offset + 4 <= extensionsEnd) {
        const quint16 extensionType = readBigEndian16(data + offset);
        const UINT extensionLength = readBigEndian16(data + offset + 2);
        offset += 4;
        if (offset + extensionLength > extensionsEnd) {
            return metadata;
        }

        if (extensionType == 0 && extensionLength >= 5) {
            UINT sniOffset = offset;
            const UINT listLength = readBigEndian16(data + sniOffset);
            sniOffset += 2;
            const UINT listEnd = sniOffset + listLength;
            if (listEnd > offset + extensionLength) {
                return metadata;
            }

            while (sniOffset + 3 <= listEnd) {
                const quint8 nameType = data[sniOffset++];
                const UINT nameLength = readBigEndian16(data + sniOffset);
                sniOffset += 2;
                if (sniOffset + nameLength > listEnd) {
                    return metadata;
                }
                if (nameType == 0) {
                    const QString hostname = QString::fromLatin1(
                        reinterpret_cast<const char *>(data + sniOffset),
                        static_cast<int>(nameLength)).toLower();
                    if (isReasonableHostname(hostname)) {
                        metadata.serverName = hostname;
                    }
                }
                sniOffset += nameLength;
            }
        } else if (extensionType == 16 && extensionLength >= 3) {
            UINT alpnOffset = offset;
            const UINT listLength = readBigEndian16(data + alpnOffset);
            alpnOffset += 2;
            const UINT listEnd = alpnOffset + listLength;
            if (listEnd <= offset + extensionLength) {
                QStringList protocols;
                while (alpnOffset + 1 <= listEnd) {
                    const UINT protocolLength = data[alpnOffset++];
                    if (alpnOffset + protocolLength > listEnd) {
                        break;
                    }
                    protocols.append(QString::fromLatin1(
                        reinterpret_cast<const char *>(data + alpnOffset),
                        static_cast<int>(protocolLength)));
                    alpnOffset += protocolLength;
                }
                metadata.alpn = protocols.join(',');
            }
        }

        offset += extensionLength;
    }

    return metadata;
}

} // namespace

WinDivertPacketCapture::WinDivertPacketCapture(QObject *parent)
    : QObject(parent)
{
}

void WinDivertPacketCapture::start()
{
    if (m_running.exchange(true)) {
        return;
    }
    m_stopRequested.store(false);

    if (QProcessEnvironment::systemEnvironment().contains("LOGIN_FORCE_WINDIVERT_MISSING")) {
        m_running.store(false);
        emit errorOccurred("WinDivert packet capture unavailable: forced missing WinDivert dependency.");
        emit statusChanged("Network packet capture degraded");
        return;
    }

    m_library = loadWinDivertLibrary();
    if (!m_library) {
        m_running.store(false);
        emit errorOccurred(QString("WinDivert packet capture unavailable: WinDivert.dll could not be loaded. LastError=%1")
                           .arg(GetLastError()));
        emit statusChanged("Network packet capture degraded");
        return;
    }

    auto openFn = reinterpret_cast<WinDivertOpenFn>(GetProcAddress(m_library, "WinDivertOpen"));
    auto recvFn = reinterpret_cast<WinDivertRecvFn>(GetProcAddress(m_library, "WinDivertRecv"));
    auto parseFn = reinterpret_cast<WinDivertHelperParsePacketFn>(GetProcAddress(m_library, "WinDivertHelperParsePacket"));
    auto ntohsFn = reinterpret_cast<WinDivertHelperNtohsFn>(GetProcAddress(m_library, "WinDivertHelperNtohs"));
    auto ntohlFn = reinterpret_cast<WinDivertHelperNtohlFn>(GetProcAddress(m_library, "WinDivertHelperNtohl"));
    m_closeFn = reinterpret_cast<WinDivertCloseFn>(GetProcAddress(m_library, "WinDivertClose"));

    if (!openFn || !recvFn || !parseFn || !ntohsFn || !ntohlFn || !m_closeFn) {
        emit errorOccurred("WinDivert packet capture unavailable: required API entry points are missing.");
        stop();
        return;
    }

    HANDLE handle = openFn("tcp or udp", WINDIVERT_LAYER_NETWORK, 0,
                           WINDIVERT_FLAG_SNIFF | WINDIVERT_FLAG_RECV_ONLY);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        m_running.store(false);
        emit errorOccurred(QString("WinDivertOpen NETWORK failed: LastError=%1. Network logging continues in degraded mode.")
                           .arg(error));
        emit statusChanged("Network packet capture degraded");
        if (m_library) {
            FreeLibrary(m_library);
            m_library = nullptr;
        }
        return;
    }

    m_handle.store(handle);
    emit statusChanged("Network packet capture active");

    QByteArray packet(static_cast<int>(WINDIVERT_MTU_MAX), 0);
    while (!m_stopRequested.load()) {
        WINDIVERT_ADDRESS address = {};
        UINT recvLen = 0;
        if (!recvFn(handle, packet.data(), static_cast<UINT>(packet.size()), &recvLen, &address)) {
            if (!m_stopRequested.load()) {
                emit errorOccurred(QString("WinDivertRecv NETWORK failed: LastError=%1").arg(GetLastError()));
            }
            break;
        }

        PWINDIVERT_IPHDR ipHeader = nullptr;
        PWINDIVERT_IPV6HDR ipv6Header = nullptr;
        PWINDIVERT_TCPHDR tcpHeader = nullptr;
        PWINDIVERT_UDPHDR udpHeader = nullptr;
        UINT8 protocol = 0;
        PVOID payload = nullptr;
        UINT payloadLen = 0;

        if (!parseFn(packet.constData(), recvLen, &ipHeader, &ipv6Header, &protocol,
                     nullptr, nullptr, &tcpHeader, &udpHeader, &payload, &payloadLen,
                     nullptr, nullptr)) {
            continue;
        }

        PacketObservation observation;
        observation.timestampUtc = QDateTime::currentDateTimeUtc();
        observation.direction = address.Outbound ? Direction::Outbound : Direction::Inbound;
        observation.transport = transportFromProtocol(protocol);
        observation.packetBytes = recvLen;
        observation.payloadBytes = payloadLen;
        observation.loopback = address.Loopback != 0;
        observation.interfaceIndex = address.Network.IfIdx;

        if (ipHeader) {
            observation.ipVersion = 4;
            observation.srcIp = IpAddress::fromV4(ntohlFn(ipHeader->SrcAddr));
            observation.dstIp = IpAddress::fromV4(ntohlFn(ipHeader->DstAddr));
        } else if (ipv6Header) {
            observation.ipVersion = 6;
            observation.srcIp = fromV6Words(ipv6Header->SrcAddr);
            observation.dstIp = fromV6Words(ipv6Header->DstAddr);
        }

        if (tcpHeader) {
            observation.srcPort = ntohsFn(tcpHeader->SrcPort);
            observation.dstPort = ntohsFn(tcpHeader->DstPort);
            observation.tcpFlags.syn = tcpHeader->Syn != 0;
            observation.tcpFlags.ack = tcpHeader->Ack != 0;
            observation.tcpFlags.fin = tcpHeader->Fin != 0;
            observation.tcpFlags.rst = tcpHeader->Rst != 0;
            if (observation.direction == Direction::Outbound && observation.dstPort == 443) {
                const TlsClientHelloMetadata tls = extractTlsClientHelloMetadata(payload, payloadLen);
                observation.visibleHostname = tls.serverName;
                observation.visibleAlpn = tls.alpn;
            }
        } else if (udpHeader) {
            observation.srcPort = ntohsFn(udpHeader->SrcPort);
            observation.dstPort = ntohsFn(udpHeader->DstPort);
        }

        emit packetObserved(observation);
    }

    closeCurrentHandle();
    if (m_library) {
        FreeLibrary(m_library);
        m_library = nullptr;
    }
    m_running.store(false);
    emit statusChanged("Network packet capture stopped");
}

void WinDivertPacketCapture::stop()
{
    m_stopRequested.store(true);
    closeCurrentHandle();
}

void WinDivertPacketCapture::closeCurrentHandle()
{
    HANDLE handle = m_handle.exchange(nullptr);
    if (handle && handle != INVALID_HANDLE_VALUE && m_closeFn) {
        m_closeFn(handle);
    }
}

} // namespace Network
