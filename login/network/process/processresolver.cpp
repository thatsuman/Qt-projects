#include "processresolver.h"

#include <QFileInfo>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Network {

namespace {

QDateTime fileTimeToDateTimeUtc(const FILETIME &fileTime)
{
    ULARGE_INTEGER value;
    value.LowPart = fileTime.dwLowDateTime;
    value.HighPart = fileTime.dwHighDateTime;

    if (value.QuadPart == 0) {
        return QDateTime();
    }

    const quint64 msecsSince1601 = value.QuadPart / 10000ULL;
    const quint64 epochDiffMsecs = 11644473600000ULL;
    if (msecsSince1601 < epochDiffMsecs) {
        return QDateTime();
    }
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(msecsSince1601 - epochDiffMsecs), Qt::UTC);
}

} // namespace

ProcessInfo ProcessResolver::resolve(quint32 pid)
{
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    const auto cached = m_cache.constFind(pid);
    if (cached != m_cache.constEnd() && cached.value().cachedUtc.secsTo(nowUtc) < m_cacheTtlSeconds) {
        return cached.value().info;
    }

    ProcessInfo info;
    info.pid = pid;

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process != nullptr) {
        wchar_t pathBuffer[MAX_PATH] = {};
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(process, 0, pathBuffer, &size)) {
            info.path = QString::fromWCharArray(pathBuffer);
            info.name = QFileInfo(info.path).fileName();
        }

        FILETIME creationTime;
        FILETIME exitTime;
        FILETIME kernelTime;
        FILETIME userTime;
        if (GetProcessTimes(process, &creationTime, &exitTime, &kernelTime, &userTime)) {
            info.creationTimeUtc = fileTimeToDateTimeUtc(creationTime);
        }

        CloseHandle(process);
    }

    if (info.name.isEmpty()) {
        info.name = "unknown";
    }

    CacheEntry entry;
    entry.info = info;
    entry.cachedUtc = nowUtc;
    m_cache.insert(pid, entry);
    return info;
}

} // namespace Network
