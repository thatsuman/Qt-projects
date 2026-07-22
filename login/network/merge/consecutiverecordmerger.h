#ifndef NETWORK_MERGE_CONSECUTIVERECORDMERGER_H
#define NETWORK_MERGE_CONSECUTIVERECORDMERGER_H

#include "network/model/networksessionrecord.h"

#include <QObject>

namespace Network {

class ConsecutiveRecordMerger : public QObject
{
    Q_OBJECT

public:
    explicit ConsecutiveRecordMerger(qint64 maxIdleGapSeconds = 300, QObject *parent = nullptr);
    ~ConsecutiveRecordMerger() override = default;

public slots:
    void processSessionRecord(const NetworkSessionRecord &record);
    void flush();

signals:
    void recordReadyForLogging(const NetworkSessionRecord &record);

private:
    bool areKeysEqual(const NetworkSessionRecord &pending, const NetworkSessionRecord &incoming) const;
    void mergeIntoPending(const NetworkSessionRecord &incoming);
    int confidenceRank(const QString &confidence) const;
    QString higherConfidence(const QString &confA, const QString &confB) const;

    NetworkSessionRecord m_pending;
    bool m_hasPending = false;
    qint64 m_maxIdleGapSeconds = 300;
};

} // namespace Network

#endif // NETWORK_MERGE_CONSECUTIVERECORDMERGER_H
