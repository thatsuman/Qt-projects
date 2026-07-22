#include "windivertpacketcapture.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QProcessEnvironment>

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
