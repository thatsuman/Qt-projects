## Context

The monitoring application emits individual `NetworkSessionRecord` instances whenever a network socket flow closes. For high-volume web browsing and HTTPS applications, this generates hundreds of near-duplicate JSONL entries for the same remote domain endpoint.

To improve log legibility and focus on high-level user activity, we introduce a post-aggregation stage that merges consecutive records sharing process identity, domain targets, and transport parameters within a 5-minute activity window.

## Goals / Non-Goals

**Goals:**
- Implement `ConsecutiveRecordMerger` to buffer and merge adjacent records sharing domain/process identity.
- Soften merge rules to absorb dynamic CDN IP rotation and DNS/SNI resolution race conditions.
- Enforce a 5-minute maximum idle gap threshold between consecutive sessions.
- Maintain thread safety and ensure full flushing on orchestrator stop or app shutdown.
- Update `NetworkSessionRecord` schema to serialize `merged_record_count` and `is_merged_consecutive_run`.

**Non-Goals:**
- Global out-of-order deduplication or reordering across non-consecutive records.
- Deep payload packet reassembly or browser history reconstruction.

## Decisions

### Decision 1: Dedicated `ConsecutiveRecordMerger` Component
- **Choice**: Create `ConsecutiveRecordMerger` class in `network/merge/consecutiverecordmerger.h|cpp`.
- **Rationale**: Keeps `FlowManager` focused on flow lifecycle and `NetworkJsonlWriter` focused on disk file I/O.
- **Alternatives Considered**: Embedding merge logic directly into `NetworkJsonlWriter` or `FlowManager`. Rejected due to mixing file I/O and flow lifecycle concerns.

### Decision 2: Domain-Centric Matching Key
- **Choice**: Match on `process.name`, `remote.hostname` (or IP fallback), `remote.port`, `transport_protocol`, and `application_layer_category`. Ignore dynamic CDN IP changes when `remote.hostname` matches.
- **Rationale**: Solves CDN IP rotation (e.g. Netflix, GitHub) and enriches sparse metadata when DNS/SNI resolution completes on subsequent connections.

### Decision 3: 5-Minute Inactivity Window Cap
- **Choice**: Enforce max idle gap of 300 seconds between consecutive connections.
- **Rationale**: Prevents multi-day merged records while keeping active browsing sessions intact.

## Risks / Trade-offs

- [Risk] Loss of granular per-socket IP logging for dynamic CDNs → Mitigation: Primary IP of first connection is retained, while hostname identity aggregates domain activity cleanly.
- [Risk] Pending record retained in memory on exit → Mitigation: Orchestrator `stop()` invokes `flush()` via `BlockingQueuedConnection` before shutting down threads.
