#ifndef NETWORK_CAPTURE_WINDIVERTFLOWCAPTURE_H
#define NETWORK_CAPTURE_WINDIVERTFLOWCAPTURE_H

#include "network/model/networkevents.h"

#include <QObject>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <atomic>

namespace Network {

class WinDivertFlowCapture : public QObject
{
    Q_OBJECT

public:
    explicit WinDivertFlowCapture(QObject *parent = nullptr);

signals:
    void flowLifecycleObserved(const FlowLifecycleObservation &observation);
    void errorOccurred(const QString &message);
    void statusChanged(const QString &status);

public slots:
    void start();
    void stop();

private:
    using WinDivertCloseFn = BOOL (WINAPI *)(HANDLE);

    void closeCurrentHandle();

    std::atomic_bool m_running{false};
    std::atomic_bool m_stopRequested{false};
    std::atomic<HANDLE> m_handle{nullptr};
    HMODULE m_library = nullptr;
    WinDivertCloseFn m_closeFn = nullptr;
};

} // namespace Network

#endif // NETWORK_CAPTURE_WINDIVERTFLOWCAPTURE_H
