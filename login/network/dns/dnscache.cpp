#include "dnscache.h"

namespace Network {

DnsCache::DnsCache(int staleWindowSeconds)
    : m_staleWindowSeconds(staleWindowSeconds)
{
}

void DnsCache::addObservation(const DnsObservation &observation)
{
    if (observation.queryName.isEmpty()) {
        return;
    }

    const QDateTime observedUtc = observation.timestampUtc.isValid()
            ? observation.timestampUtc.toUTC()
            : QDateTime::currentDateTimeUtc();
    const QDateTime expiresUtc = observedUtc.addSecs(qMax(1, observation.ttlSeconds));

    for (const IpAddress &ip : observation.answerIps) {
        Candidate candidate;
        candidate.hostname = observation.queryName;
        candidate.pid = observation.pid;
        candidate.observedUtc = observedUtc;
        candidate.expiresUtc = expiresUtc;
        candidate.source = observation.source;

        QList<Candidate> &candidates = m_candidatesByIp[ip];
        bool replaced = false;
        for (Candidate &existing : candidates) {
            if (existing.hostname == candidate.hostname && existing.pid == candidate.pid) {
                existing = candidate;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            candidates.append(candidate);
        }
    }
}

HostnameAttribution DnsCache::lookup(const IpAddress &remoteIp, quint32 pid, const QDateTime &flowStartTimeUtc)
{
    HostnameAttribution result;
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    evictExpired(nowUtc);

    const QList<Candidate> candidates = m_candidatesByIp.value(remoteIp);
    if (candidates.isEmpty()) {
        return result;
    }

    int bestScore = -1;
    Candidate best;
    QStringList names;

    for (const Candidate &candidate : candidates) {
        if (!names.contains(candidate.hostname)) {
            names.append(candidate.hostname);
        }

        int score = 0;
        if (candidate.expiresUtc >= nowUtc) {
            score += 20;
        } else if (candidate.expiresUtc.addSecs(m_staleWindowSeconds) >= nowUtc) {
            score += 5;
        }

        if (pid != 0 && candidate.pid == pid) {
            score += 20;
        }

        if (!flowStartTimeUtc.isValid() || candidate.observedUtc <= flowStartTimeUtc.addSecs(5)) {
            score += 5;
        }

        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    }

    result.primaryName = best.hostname;
    result.candidates = names;
    result.source = best.source.isEmpty() ? "etw_dns" : best.source;

    if (best.expiresUtc < nowUtc) {
        result.confidence = "low";
    } else if (pid != 0 && best.pid == pid) {
        result.confidence = "high";
    } else {
        result.confidence = "medium";
    }

    return result;
}

void DnsCache::evictExpired(const QDateTime &nowUtc)
{
    auto it = m_candidatesByIp.begin();
    while (it != m_candidatesByIp.end()) {
        QList<Candidate> kept;
        for (const Candidate &candidate : it.value()) {
            if (candidate.expiresUtc.addSecs(m_staleWindowSeconds) >= nowUtc) {
                kept.append(candidate);
            }
        }

        if (kept.isEmpty()) {
            it = m_candidatesByIp.erase(it);
        } else {
            it.value() = kept;
            ++it;
        }
    }
}

} // namespace Network
