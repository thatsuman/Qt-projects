## Why

The current network logging implementation outputs individual socket session records for every short-lived connection, leading to noisy and unaggregated `network_log.jsonl` files. Merging consecutive records that represent the same logical web browsing session or remote endpoint communication significantly reduces log volume while providing cleaner, domain-centric user activity logs.

## What Changes

- Implement a **Domain-Centric Consecutive Record Merger** (`ConsecutiveRecordMerger`) inserted between `FlowManager` and `NetworkJsonlWriter`.
- Merge consecutive network records matching on `process.name`, `remote.hostname` (or IP fallback), `remote.port`, and `transport_protocol`.
- Enrich sparse metadata during merge (e.g. absorbing non-empty hostname or known application protocol hints).
- Aggregate metrics: sum `bytesSentTotal`, `bytesReceivedTotal`, `packetsSent`, `packetsReceived`.
- Enforce an idle gap threshold (max 5 minutes of inactivity between consecutive connections) to split distinct browsing sessions.
- Update output JSON schema to include `merged_record_count` and `is_merged_consecutive_run`.

## Capabilities

### New Capabilities
- `consecutive-network-record-merging`: Aggregates consecutive network session records sharing process and domain targets into unified session records with metric summation, confidence resolution, and time gap bounding.

### Modified Capabilities
- none

## Impact

- **Affected Code**: `network/merge/consecutiverecordmerger.h|cpp`, `network/model/networksessionrecord.h`, `network/orchestrator/networkorchestrator.h|cpp`, `network/writer/networkjsonlwriter.h|cpp`, `login.pro`.
- **APIs/Output**: `network_log.jsonl` output schema enhanced with `merged_record_count` and `is_merged_consecutive_run`.
- **Dependencies**: No external library additions. Standard Qt C++17.
