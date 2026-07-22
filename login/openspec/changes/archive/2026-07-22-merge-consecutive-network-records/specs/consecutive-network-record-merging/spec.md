## ADDED Requirements

### Requirement: Consecutive Network Record Merging
The system SHALL aggregate consecutive network session records produced by FlowManager into a single merged session record when the records belong to the same logical remote endpoint and process within a maximum idle gap threshold.

#### Scenario: Consecutive matching domain connections are merged
- **WHEN** multiple consecutive NetworkSessionRecords arrive with matching process.name, matching remote.hostname (or matching remote.ip if hostname is empty), matching remote.port, matching transport_protocol, and time gap <= 300 seconds
- **THEN** the system merges them into a single pending record where start_time_utc is preserved from the first record, end_time_utc is updated from the last record, bytes and packet counts are summed, and merged_record_count is incremented

#### Scenario: Non-consecutive connections are not merged
- **WHEN** a record arrives with a different domain identity key or process name
- **THEN** the system flushes the previous pending merged record to NetworkJsonlWriter and begins a new pending record

#### Scenario: Inactivity gap splits sessions
- **WHEN** two consecutive records match identity keys but the time difference between the end of the first and start of the second exceeds 300 seconds (5 minutes)
- **THEN** the system flushes the first record and starts a new pending record for the second connection

#### Scenario: Shutdown flush writes pending merged record
- **WHEN** the network orchestrator or application stops
- **THEN** ConsecutiveRecordMerger flushes its pending record to NetworkJsonlWriter ensuring no records are lost
