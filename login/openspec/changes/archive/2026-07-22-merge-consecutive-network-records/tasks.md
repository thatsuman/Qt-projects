## 1. Model & Schema Enhancements

- [x] 1.1 Update `NetworkSessionRecord` struct in `network/model/networksessionrecord.h` to add `mergedRecordCount` (default 1) and `isMergedConsecutiveRun` fields.
- [x] 1.2 Update `NetworkSessionRecord::toJson()` to serialize `merged_record_count` and `is_merged_consecutive_run`.

## 2. Merger Core Implementation

- [x] 2.1 Create header `network/merge/consecutiverecordmerger.h` with signals/slots for processing sessions, key matching, metadata enrichment, confidence resolution, and flushing.
- [x] 2.2 Create implementation `network/merge/consecutiverecordmerger.cpp` with domain-centric equality logic, 5-minute time gap check, metric summation, and sticky `app_shutdown` close reason resolution.

## 3. Orchestrator Integration

- [x] 3.1 Update `NetworkOrchestrator` in `network/orchestrator/networkorchestrator.h|cpp` to instantiate `ConsecutiveRecordMerger`.
- [x] 3.2 Wire signal `FlowManager::sessionClosed` to `ConsecutiveRecordMerger::processSessionRecord` and `ConsecutiveRecordMerger::recordReadyForLogging` to `NetworkJsonlWriter::writeSession`.
- [x] 3.3 Ensure `NetworkOrchestrator::stop()` invokes `m_merger->flush()` via blocking connection before stopping worker threads.
- [x] 3.4 Update `login.pro` to include `network/merge/consecutiverecordmerger.h` and `network/merge/consecutiverecordmerger.cpp`.

## 4. Verification & Testing

- [x] 4.1 Build project using `qmake` / `nmake` / `do_build.bat`.
- [x] 4.2 Verify unit scenarios: matching consecutive domain records merge, non-consecutive records do not merge, time gap > 5 mins splits records, and shutdown flush flushes final pending record.
- [x] 4.3 Inspect output `network_log.jsonl` to confirm correct `merged_record_count` and domain-centric aggregation.
