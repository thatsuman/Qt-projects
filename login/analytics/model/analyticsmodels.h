#ifndef ANALYTICS_MODEL_ANALYTICSMODELS_H
#define ANALYTICS_MODEL_ANALYTICSMODELS_H

#include <QDateTime>
#include <QString>
#include <QStringList>

namespace Analytics {

// ── ActivityRecord ────────────────────────────────────────────────────────────
// Normalized record from one parsed line of activity_log.jsonl.
// Raw keystroke text is NEVER stored here — only the character count.
struct ActivityRecord
{
    QDateTime startTime;
    QDateTime endTime;
    QString   windowTitle;
    QString   processName;
    int       keystrokeCount   = 0;   // length of raw keystroke string, discarded immediately
    double    mouseDistancePx  = 0.0;

    qint64 durationSeconds() const
    {
        if (!startTime.isValid() || !endTime.isValid()) return 0;
        return startTime.secsTo(endTime);
    }
};

// ── NetworkRecord ─────────────────────────────────────────────────────────────
// Normalized record from one parsed line of network_log.jsonl.
struct NetworkRecord
{
    QDateTime startTime;
    QDateTime endTime;

    // Remote endpoint
    QString remoteIp;
    int     remotePort        = 0;
    QString remoteHostname;           // empty if not attributed
    QString hostnameConfidence;       // "high", "medium", "low", "none"

    // Process attribution
    quint32 processPid        = 0;
    QString processName;              // "unknown" if not attributed
    QString processConfidence;        // "high", "medium", "low", "none"
    QString processSource;            // "windivert_flow", "iphelper", "etw_dns", "unknown"

    // Protocol
    QString transportProtocol;        // "TCP", "UDP", "ICMP"
    QString appProtocolHint;          // "HTTPS", "HTTP", "DNS", "QUIC/HTTP3", etc.
    QString applicationLayerCategory;

    // Traffic volume
    quint64 bytesSent     = 0;
    quint64 bytesReceived = 0;
    quint64 packetsSent   = 0;
    quint64 packetsReceived = 0;

    // Metadata
    QString closeReason;
    int     mergedRecordCount = 1;

    // DNS info (only for DNS resolver sessions)
    QString dnsQueryName;
    QStringList dnsAnswerIps;

    quint64 totalBytes() const { return bytesSent + bytesReceived; }

    // The display hostname: prefer attributed hostname, fall back to IP
    QString displayHost() const
    {
        return remoteHostname.isEmpty() ? remoteIp : remoteHostname;
    }
};

// ── ErrorRecord ───────────────────────────────────────────────────────────────
// Represents a parsed line from network_error.txt.
struct ErrorRecord
{
    QDateTime timestamp;
    QString   message;
    bool      isDnsWarning    = false;
    bool      isShutdownFlush = false;
};

// ── DiagRecord ────────────────────────────────────────────────────────────────
// Represents a parsed entry from network_dns_diag.jsonl.
struct DiagRecord
{
    QDateTime timestamp;
    int       eventId         = 0;
    QStringList propertyNames;
};

} // namespace Analytics

#endif // ANALYTICS_MODEL_ANALYTICSMODELS_H
