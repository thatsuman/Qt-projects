## ADDED Requirements

### Requirement: Mandatory administrator elevation

The application SHALL require Windows administrator (elevated) privileges before displaying any UI or starting any subsystem.

#### Scenario: UAC prompt on launch
- **WHEN** the application executable is launched
- **THEN** Windows SHALL display a User Account Control (UAC) elevation prompt via the embedded application manifest (`requestedExecutionLevel = requireAdministrator`).
- **AND IF** the user accepts, the application SHALL start normally.
- **AND IF** the user rejects, the application SHALL NOT start.

#### Scenario: Runtime fallback check
- **WHEN** the application starts without elevation (e.g., manifest stripped or compatibility mode)
- **THEN** the application SHALL display an error dialog stating administrator privileges are required
- **AND** SHALL exit with a non-zero return code without showing the main window.

### Requirement: Separate network module lifecycle

The network logging module SHALL be completely separate from the existing ActivityLogger module. MainWindow SHALL orchestrate only lifecycle calls (start/stop) for the NetworkOrchestrator.

#### Scenario: Start on login
- **WHEN** a user successfully authenticates
- **THEN** the system SHALL start the NetworkOrchestrator with the authenticated username.

#### Scenario: Stop on logout
- **WHEN** the user logs out
- **THEN** the system SHALL stop the NetworkOrchestrator before stopping ActivityLogger and hooks.

#### Scenario: Stop on close
- **WHEN** the application window is closed while a user is logged in
- **THEN** the system SHALL stop the NetworkOrchestrator before stopping ActivityLogger and hooks.

### Requirement: Passive packet capture

The system SHALL capture TCP and UDP packet headers using WinDivert in a purely passive mode. Packets SHALL NOT be modified, dropped, or reinjected.

#### Scenario: SNIFF + RECV_ONLY mode
- **WHEN** the packet capture subsystem starts
- **THEN** it SHALL open a WinDivert handle with `WINDIVERT_FLAG_SNIFF | WINDIVERT_FLAG_RECV_ONLY` on the NETWORK layer.
- **AND** it SHALL parse IP and TCP/UDP headers only — no payload inspection.

#### Scenario: Capture failure
- **WHEN** `WinDivertOpen` fails (e.g., driver missing, access denied)
- **THEN** the system SHALL log the error to `network_error.txt`
- **AND** SHALL continue operating without packet capture (degraded mode).

### Requirement: Packet-to-flow aggregation

The FlowManager SHALL aggregate individual packet observations into flow-level session records identified by a stable FlowKey.

#### Scenario: FlowKey normalization
- **GIVEN** a packet observation with source and destination endpoints
- **WHEN** the direction is outbound
- **THEN** the FlowKey SHALL use source as local and destination as remote.
- **WHEN** the direction is inbound
- **THEN** the FlowKey SHALL use destination as local and source as remote.

#### Scenario: FlowKey composition
- **THEN** the FlowKey SHALL consist of: `localIp`, `localPort`, `remoteIp`, `remotePort`, `transport` (TCP/UDP/ICMP/Other), and `ipVersion` (4 or 6).
- **AND** the FlowKey SHALL NOT include domain name or process name.

#### Scenario: Session tracking
- **WHEN** packets are observed for a flow
- **THEN** the FlowSession SHALL track: bytes sent/received, packets sent/received, first/last seen timestamps, TCP flags (SYN/FIN/RST), and direction.

### Requirement: Flow close policy

The system SHALL close and emit a final NetworkSessionRecord when any of the following conditions are met:

#### Scenario: TCP RST
- **WHEN** a TCP RST flag is observed → close reason: `tcp_rst`.

#### Scenario: TCP FIN + grace
- **WHEN** a TCP FIN flag is observed and no further packets arrive within a configurable grace window (default 5 seconds) → close reason: `tcp_fin`.

#### Scenario: WinDivert flow deletion
- **WHEN** a WinDivert FLOW layer deletion event arrives for the tuple → close reason: `flow_deleted`.

#### Scenario: Idle timeout
- **WHEN** a TCP flow has been idle for longer than a configurable timeout (default 120 seconds) → close reason: `idle_timeout`.
- **WHEN** a UDP flow has been idle for longer than a configurable timeout (default 30 seconds) → close reason: `idle_timeout`.

#### Scenario: App shutdown
- **WHEN** the user logs out or the application closes → close reason: `app_shutdown`. All active flows SHALL be flushed.

### Requirement: DNS hostname correlation via ETW

The system SHALL correlate DNS Client ETW events to enrich network sessions with hostname/domain information.

#### Scenario: ETW DNS Client session
- **WHEN** the DNS monitor starts
- **THEN** it SHALL start a real-time ETW session for the `Microsoft-Windows-DNS-Client` provider.
- **AND** it SHALL extract: query name, answer IPs, PID (when available), and timestamp from DNS events.
- **AND** it SHALL NOT mandate specific Event IDs — the implementation starts with known IDs and adds parsers over time.

#### Scenario: Unknown event handling
- **WHEN** an unrecognized DNS Client event ID is received
- **THEN** the system SHALL log the event ID and available schema/properties to `network_error.txt` for future parser development.

#### Scenario: ETW failure
- **WHEN** the ETW session fails to start
- **THEN** the system SHALL log the error and continue — sessions will have `hostname: null`.

### Requirement: DNS cache with stale window

The DnsCache SHALL store IP-to-hostname mappings with TTL-aware expiry and a configurable stale window.

#### Scenario: Multiple candidates
- **GIVEN** the same IP address is resolved to different hostnames by different processes or at different times
- **THEN** the cache SHALL store multiple hostname candidates per IP.

#### Scenario: TTL + stale window
- **WHEN** a cache entry's TTL expires
- **THEN** the entry SHALL be retained in a stale state for a configurable stale window period with reduced confidence.
- **WHEN** the stale window also expires
- **THEN** the entry SHALL be evicted.

### Requirement: Process attribution

The system SHALL attribute network sessions to processes with confidence levels.

#### Scenario: Attribution priority
- **GIVEN** multiple sources can identify a process for a connection
- **THEN** the system SHALL prefer: WinDivert FLOW/SOCKET PID (high confidence) > IP Helper snapshot PID (medium confidence) > DNS event PID as weak hint (low confidence) > unknown (none).

### Requirement: Hostname attribution

The system SHALL attribute hostnames to network sessions with confidence levels.

#### Scenario: Attribution priority
- **GIVEN** multiple DNS cache entries could match a remote IP
- **THEN** the system SHALL prefer: DNS answer IP + same PID + recent + TTL valid (high) > DNS answer IP + recent + no PID match (medium) > DNS answer IP + stale (low) > reverse DNS if enabled (low) > unknown (none).

### Requirement: Application protocol hinting

The system SHALL infer application protocol hints based on transport protocol and port numbers.

#### Scenario: Protocol hint format
- **WHEN** a session is closed
- **THEN** the record SHALL include `app_protocol_hint`, `app_protocol_confidence`, and `app_protocol_reason` fields.
- **AND** the transport protocol (TCP/UDP/ICMP) SHALL always be recorded as ground truth.
- **AND** the system SHALL NOT claim REST, GraphQL, gRPC, or WebSocket for encrypted traffic.

### Requirement: JSONL network session output

The system SHALL write completed network session records to `logs/<username>/network_log.jsonl` in JSON Lines format.

#### Scenario: Record fields
- **WHEN** a network session record is written
- **THEN** it SHALL include: `type` ("network_session"), `schema_version` (1), `username`, `flow_id`, `start_time_utc`, `end_time_utc`, `duration_ms`, `local` (ip, port), `remote` (ip, port, hostname, hostname_source, hostname_confidence, hostname_candidates), `process` (pid, name, path, source, confidence), `transport_protocol`, `app_protocol_hint`, `app_protocol_confidence`, `app_protocol_reason`, `bytes` (sent_total, received_total, sent_payload, received_payload), `packets` (sent, received), `close_reason`, and `flags` (ipv6, loopback).

#### Scenario: Error diagnostics
- **WHEN** a network subsystem error occurs
- **THEN** it SHALL be logged to `logs/<username>/network_error.txt` with a timestamp and error description.

#### Scenario: File creation timing
- **THEN** `network_log.jsonl` SHALL be created only after successful login, not at app startup.

### Requirement: Degraded-mode behavior

Each network subsystem (WinDivert packet, WinDivert flow, ETW DNS, IP Helper) SHALL fail independently. A failure in one subsystem SHALL NOT prevent other subsystems or the existing ActivityLogger from operating.

### Requirement: Shutdown flushing

On logout or application close, the system SHALL: stop capture sources first (no new events), stop ETW and IP Helper, flush all active sessions from FlowManager with `close_reason = app_shutdown`, flush and close the writer, and join worker threads cleanly.

### Requirement: Threading

All long-running or blocking network operations SHALL run off the UI thread. The FlowManager SHALL run on its own dedicated thread. ETW `ProcessTrace` and IP Helper polling SHALL NOT run in the FlowManager thread — each SHALL have its own thread.

### Requirement: No privacy-violating data

The system SHALL NOT log: browser history, decrypted payloads, HTTP bodies, request/response payloads, credentials, cookies, tokens, or any data obtained via TLS decryption or MITM proxying.
