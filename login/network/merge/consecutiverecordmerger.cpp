#include "consecutiverecordmerger.h"

#include <QtGlobal>

namespace Network {

ConsecutiveRecordMerger::ConsecutiveRecordMerger(qint64 maxIdleGapSeconds, QObject *parent)
    : QObject(parent)
    , m_maxIdleGapSeconds(maxIdleGapSeconds)
{
}

void ConsecutiveRecordMerger::processSessionRecord(const NetworkSessionRecord &incoming)
{
    if (!m_hasPending) {
        m_pending = incoming;
        m_hasPending = true;
        return;
    }

    if (areKeysEqual(m_pending, incoming)) {
        mergeIntoPending(incoming);
    } else {
        emit recordReadyForLogging(m_pending);
        m_pending = incoming;
    }
}

void ConsecutiveRecordMerger::flush()
{
    if (m_hasPending) {
        emit recordReadyForLogging(m_pending);
        m_hasPending = false;
        m_pending = NetworkSessionRecord();
    }
}

bool ConsecutiveRecordMerger::areKeysEqual(const NetworkSessionRecord &pending, const NetworkSessionRecord &incoming) const
{
    // 1. Process name matching (case-insensitive)
    if (pending.process.name.compare(incoming.process.name, Qt::CaseInsensitive) != 0) {
        return false;
    }

    // 2. Transport protocol and server port match
    if (pending.key.transport != incoming.key.transport || pending.key.remotePort != incoming.key.remotePort) {
        return false;
    }

    // 3. Application layer category match (if both specified and not "unknown")
    if (!pending.applicationLayerCategory.isEmpty() && pending.applicationLayerCategory != "unknown" &&
        !incoming.applicationLayerCategory.isEmpty() && incoming.applicationLayerCategory != "unknown") {
        if (pending.applicationLayerCategory != incoming.applicationLayerCategory) {
            return false;
        }
    }

    // 4. Domain & Target Endpoint Match
    const bool pendingHasHost = !pending.remoteHost.primaryName.isEmpty();
    const bool incomingHasHost = !incoming.remoteHost.primaryName.isEmpty();

    if (pendingHasHost && incomingHasHost) {
        // Both have hostnames: compare hostnames (supports CDN IP rotation)
        if (pending.remoteHost.primaryName.compare(incoming.remoteHost.primaryName, Qt::CaseInsensitive) != 0) {
            return false;
        }
    } else {
        // At least one is missing hostname: compare IP address
        if (pending.key.remoteIp != incoming.key.remoteIp) {
            return false;
        }
    }

    // 5. Time Gap Threshold Check (Max idle gap in seconds)
    if (pending.endTimeUtc.isValid() && incoming.startTimeUtc.isValid()) {
        const qint64 gapSeconds = pending.endTimeUtc.secsTo(incoming.startTimeUtc);
        if (gapSeconds > m_maxIdleGapSeconds) {
            return false;
        }
    }

    return true;
}

void ConsecutiveRecordMerger::mergeIntoPending(const NetworkSessionRecord &incoming)
{
    // Preserve earliest start_time, update latest end_time
    if (incoming.endTimeUtc.isValid() && incoming.endTimeUtc > m_pending.endTimeUtc) {
        m_pending.endTimeUtc = incoming.endTimeUtc;
    }

    // Sum network transfer metrics
    m_pending.bytesSentTotal += incoming.bytesSentTotal;
    m_pending.bytesReceivedTotal += incoming.bytesReceivedTotal;
    m_pending.payloadBytesSent += incoming.payloadBytesSent;
    m_pending.payloadBytesReceived += incoming.payloadBytesReceived;
    m_pending.packetsSent += incoming.packetsSent;
    m_pending.packetsReceived += incoming.packetsReceived;

    // Increment merge counts
    m_pending.mergedRecordCount += incoming.mergedRecordCount;
    m_pending.isMergedConsecutiveRun = true;

    // Hostname enrichment & confidence
    if (m_pending.remoteHost.primaryName.isEmpty() && !incoming.remoteHost.primaryName.isEmpty()) {
        m_pending.remoteHost = incoming.remoteHost;
    } else if (!incoming.remoteHost.primaryName.isEmpty()) {
        if (confidenceRank(incoming.remoteHost.confidence) > confidenceRank(m_pending.remoteHost.confidence)) {
            m_pending.remoteHost = incoming.remoteHost;
        }
    }

    // Process confidence
    m_pending.process.confidence = higherConfidence(m_pending.process.confidence, incoming.process.confidence);

    // Protocol hint enrichment
    if ((m_pending.appProtocol.hint.isEmpty() || m_pending.appProtocol.hint == "unknown") &&
        !incoming.appProtocol.hint.isEmpty() && incoming.appProtocol.hint != "unknown") {
        m_pending.appProtocol = incoming.appProtocol;
    }

    // Application layer category enrichment
    if ((m_pending.applicationLayerCategory.isEmpty() || m_pending.applicationLayerCategory == "unknown") &&
        !incoming.applicationLayerCategory.isEmpty() && incoming.applicationLayerCategory != "unknown") {
        m_pending.applicationLayerCategory = incoming.applicationLayerCategory;
    }

    // Sticky close_reason: app_shutdown takes precedence, otherwise latest record's close reason
    if (m_pending.closeReason == "app_shutdown" || incoming.closeReason == "app_shutdown") {
        m_pending.closeReason = "app_shutdown";
    } else if (!incoming.closeReason.isEmpty()) {
        m_pending.closeReason = incoming.closeReason;
    }
}

int ConsecutiveRecordMerger::confidenceRank(const QString &confidence) const
{
    const QString conf = confidence.toLower();
    if (conf == "high") return 4;
    if (conf == "medium") return 3;
    if (conf == "low") return 2;
    if (conf == "none") return 1;
    return 0; // "unknown" or empty
}

QString ConsecutiveRecordMerger::higherConfidence(const QString &confA, const QString &confB) const
{
    return (confidenceRank(confA) >= confidenceRank(confB)) ? confA : confB;
}

} // namespace Network
