#include "windivertflowcapture.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QProcessEnvironment>

#include "windivert.h"

namespace Network {

namespace {

using WinDivertOpenFn = HANDLE (WINAPI *)(const char *, WINDIVERT_LAYER, INT16, UINT64);
using WinDivertRecvFn = BOOL (WINAPI *)(HANDLE, VOID *, UINT, UINT *, WINDIVERT_ADDRESS *);
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

IpAddress flowAddressToIp(const UINT32 *address, bool ipv6, WinDivertHelperNtohlFn ntohlFn)
{
    if (ipv6) {
        return IpAddress::fromV6(reinterpret_cast<const quint8 *>(address));
    }
    return IpAddress::fromV4(ntohlFn(address[0]));
}

} // namespace

WinDivertFlowCapture::WinDivertFlowCapture(QObject *parent)
    : QObject(parent)
{
}

void WinDivertFlowCapture::start()
{
    if (m_running.exchange(true)) {
        return;
    }
    m_stopRequested.store(false);

    if (QProcessEnvironment::systemEnvironment().contains("LOGIN_FORCE_WINDIVERT_MISSING")) {
        m_running.store(false);
        emit errorOccurred("WinDivert flow capture unavailable: forced missing WinDivert dependency.");
        emit statusChanged("Network flow capture degraded");
        return;
    }

    m_library = loadWinDivertLibrary();
    if (!m_library) {
        m_running.store(false);
        emit errorOccurred(QString("WinDivert flow capture unavailable: WinDivert.dll could not be loaded. LastError=%1")
                           .arg(GetLastError()));
        emit statusChanged("Network flow capture degraded");
        return;
    }

    auto openFn = reinterpret_cast<WinDivertOpenFn>(GetProcAddress(m_library, "WinDivertOpen"));
    auto recvFn = reinterpret_cast<WinDivertRecvFn>(GetProcAddress(m_library, "WinDivertRecv"));
    auto ntohsFn = reinterpret_cast<WinDivertHelperNtohsFn>(GetProcAddress(m_library, "WinDivertHelperNtohs"));
    auto ntohlFn = reinterpret_cast<WinDivertHelperNtohlFn>(GetProcAddress(m_library, "WinDivertHelperNtohl"));
    m_closeFn = reinterpret_cast<WinDivertCloseFn>(GetProcAddress(m_library, "WinDivertClose"));

    if (!openFn || !recvFn || !ntohsFn || !ntohlFn || !m_closeFn) {
        emit errorOccurred("WinDivert flow capture unavailable: required API entry points are missing.");
        stop();
        return;
    }

    HANDLE handle = openFn("true", WINDIVERT_LAYER_FLOW, 0,
                           WINDIVERT_FLAG_SNIFF | WINDIVERT_FLAG_RECV_ONLY);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        m_running.store(false);
        emit errorOccurred(QString("WinDivertOpen FLOW failed: LastError=%1. Process attribution will rely on IP Helper snapshots.")
                           .arg(error));
        emit statusChanged("Network flow capture degraded");
        if (m_library) {
            FreeLibrary(m_library);
            m_library = nullptr;
        }
        return;
    }

    m_handle.store(handle);
    emit statusChanged("Network flow capture active");

    while (!m_stopRequested.load()) {
        WINDIVERT_ADDRESS address = {};
        UINT recvLen = 0;
        if (!recvFn(handle, nullptr, 0, &recvLen, &address)) {
            if (!m_stopRequested.load()) {
                emit errorOccurred(QString("WinDivertRecv FLOW failed: LastError=%1").arg(GetLastError()));
            }
            break;
        }

        if (address.Event != WINDIVERT_EVENT_FLOW_ESTABLISHED
                && address.Event != WINDIVERT_EVENT_FLOW_DELETED) {
            continue;
        }

        FlowLifecycleObservation observation;
        observation.timestampUtc = QDateTime::currentDateTimeUtc();
        observation.event = address.Event == WINDIVERT_EVENT_FLOW_DELETED
                ? FlowLifecycleEvent::Deleted
                : FlowLifecycleEvent::Established;
        observation.pid = address.Flow.ProcessId;
        observation.source = "windivert_flow";
        observation.key.localIp = flowAddressToIp(address.Flow.LocalAddr, address.IPv6 != 0, ntohlFn);
        observation.key.remoteIp = flowAddressToIp(address.Flow.RemoteAddr, address.IPv6 != 0, ntohlFn);
        observation.key.localPort = ntohsFn(address.Flow.LocalPort);
        observation.key.remotePort = ntohsFn(address.Flow.RemotePort);
        observation.key.transport = transportFromProtocol(address.Flow.Protocol);
        observation.key.ipVersion = address.IPv6 ? 6 : 4;

        emit flowLifecycleObserved(observation);
    }

    closeCurrentHandle();
    if (m_library) {
        FreeLibrary(m_library);
        m_library = nullptr;
    }
    m_running.store(false);
    emit statusChanged("Network flow capture stopped");
}

void WinDivertFlowCapture::stop()
{
    m_stopRequested.store(true);
    closeCurrentHandle();
}

void WinDivertFlowCapture::closeCurrentHandle()
{
    HANDLE handle = m_handle.exchange(nullptr);
    if (handle && handle != INVALID_HANDLE_VALUE && m_closeFn) {
        m_closeFn(handle);
    }
}

} // namespace Network
