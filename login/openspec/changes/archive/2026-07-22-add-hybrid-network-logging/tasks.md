## Phase 0: OpenSpec Artifacts (This Change)

- [x] 0.1 Create `openspec/changes/add-hybrid-network-logging/proposal.md` - change proposal.
- [x] 0.2 Create `openspec/changes/add-hybrid-network-logging/design.md` - architecture decisions.
- [x] 0.3 Create `openspec/changes/add-hybrid-network-logging/tasks.md` - this task list.
- [x] 0.4 Create `openspec/changes/add-hybrid-network-logging/specs/network-logging/spec.md` - formal requirements.
- [x] 0.5 Create `login.manifest` with `requestedExecutionLevel = requireAdministrator`.
- [x] 0.6 Create `login.rc` resource file embedding the manifest.
- [x] 0.7 Add `RC_FILE = login.rc` to `login.pro`.
- [x] 0.8 Add runtime admin check in `main.cpp` using `CheckTokenMembership` - show error dialog and exit if not elevated.

## Phase 1: Pure Model + FlowManager with Fake Events

- [x] 1.1 Create `network/model/networkevents.h` - define `PacketObservation`, `FlowLifecycleObservation`, `ProcessConnectionSnapshot`, `DnsObservation` data structs.
- [x] 1.2 Create `network/model/flowkey.h` - define `IpAddress` custom type (`std::array<uint8_t, 16>`) with `toString()`, `fromV4()`, `fromV6()`, `operator==`, and `qHash()`. Define `FlowKey` struct with `localIp`, `localPort`, `remoteIp`, `remotePort`, `transport`, `ipVersion`, plus equality and hash.
- [x] 1.3 Create `network/model/flowsession.h` - define `FlowSession` with all tracking fields (bytes, packets, TCP flags, process info, hostname candidates, protocol hint, close reason).
- [x] 1.4 Create `network/model/networksessionrecord.h` - define `NetworkSessionRecord` as the final JSON-ready struct with a `toJson()` method returning `QJsonObject`.
- [x] 1.5 Create `network/flow/flowmanager.h` and `network/flow/flowmanager.cpp` - implement FlowManager as a `QObject` that: receives events via slots, maintains `QHash<FlowKey, FlowSession>`, tracks idle timeouts via `QTimer`, emits `sessionClosed(NetworkSessionRecord)` on flow close, and implements `flushAll(closeReason)` for shutdown.
- [x] 1.6 Implement FlowKey normalization logic: outbound -> local=src, remote=dst; inbound -> local=dst, remote=src.
- [x] 1.7 Implement flow close policy: TCP RST, TCP FIN + grace (5s), flow deleted event, TCP idle > 120s, UDP idle > 30s, app shutdown.
- [x] 1.8 Create `test_flowmanager/` test executable - feed synthetic `PacketObservation` events and validate emitted `NetworkSessionRecord` JSON output.
- [x] 1.9 Add Phase 1 source/header files to `login.pro` (or a separate `.pro` for the test executable).
- [x] 1.10 Build and verify Phase 1 compiles cleanly. Run test executable, validate JSON output.

## Phase 2: NetworkJsonlWriter

- [x] 2.1 Create `network/writer/networkjsonlwriter.h` and `network/writer/networkjsonlwriter.cpp` - QObject that receives `NetworkSessionRecord` via queued signal, writes JSONL to `logs/<username>/network_log.jsonl`.
- [x] 2.2 Implement `start(username)` - create `logs/<username>/` directory, open log file in append mode.
- [x] 2.3 Implement `stop()` - flush all pending writes, close file handles.
- [x] 2.4 Implement error logging to `logs/<username>/network_error.txt` with timestamped entries.
- [x] 2.5 Add writer source/header files to `login.pro`.
- [x] 2.6 Extend test executable to wire FlowManager -> Writer and verify JSONL file output.

## Phase 3: WinDivert Packet Capture

- [x] 3.1 Vendor WinDivert 2.2.2 into `third_party/WinDivert/` - include headers, x64 lib, DLL, signed .sys driver, and LGPL license file.
- [x] 3.2 Add WinDivert include path to `login.pro` and keep WinDivert loaded dynamically at runtime so missing DLL/SYS dependencies degrade gracefully instead of preventing app launch.
- [x] 3.3 Create `network/capture/windivertpacketcapture.h` and `.cpp` - open WinDivert handle with `WINDIVERT_FLAG_SNIFF | WINDIVERT_FLAG_RECV_ONLY` on NETWORK layer. Blocking `WinDivertRecv` loop on its own QThread.
- [x] 3.4 Parse IP + TCP/UDP headers only (no payload). Build `PacketObservation` and emit `packetObserved` signal.
- [x] 3.5 Implement RAII handle wrapper - `WinDivertClose` in destructor and stop method.
- [x] 3.6 Handle `WinDivertOpen` failure gracefully - log error, emit degraded status, continue without capture.
- [x] 3.7 Add capture source/header files to `login.pro`.
- [x] 3.8 Build and verify. Test with elevated privileges - confirm packets are observed without traffic disruption.

## Phase 4: Flow Lifecycle + PID Enrichment

- [x] 4.1 Create `network/capture/windivertflowcapture.h` and `.cpp` - WinDivert FLOW layer capture on its own QThread. Emit `FlowLifecycleObservation` (established/deleted + PID) via signal.
- [x] 4.2 Create `network/process/processresolver.h` and `.cpp` - PID-to-(name, path) cache with short TTL. Uses `OpenProcess` + `QueryFullProcessImageNameW`. Captures PID creation time to support reuse checks.
- [x] 4.3 Create `network/process/iphelperconnectionpoller.h` and `.cpp` - polls `GetExtendedTcpTable`/`GetExtendedUdpTable` with owner PID tables via `QTimer` on its own QThread. Initial poll interval: 1 second. Emits `ProcessConnectionSnapshot` via signal.
- [x] 4.4 Add `iphlpapi.lib` to `login.pro` LIBS.
- [x] 4.5 Wire FlowLifecycleObservation and ProcessConnectionSnapshot into FlowManager via queued connections. Implement process attribution priority: WinDivert FLOW PID (high) > IP Helper PID (medium) > DNS event PID (low) > unknown.
- [x] 4.6 Add Phase 4 source/header files to `login.pro`.
- [x] 4.7 Build and verify. Confirm flow sessions show process name/path with correct confidence levels.

## Phase 5: ETW DNS + DNS Cache

- [x] 5.1 Create `network/dns/etwdnsmonitor.h` and `.cpp` - start real-time ETW session for `Microsoft-Windows-DNS-Client` provider. `ProcessTrace` runs in its own dedicated QThread (blocking call). Parse DNS events to extract query name, answer IPs, PID, timestamp. Log unrecognized event IDs and their TDH-enumerated properties to `network_error.txt`. Emit `DnsObservation` via queued signal.
- [x] 5.2 Create `network/dns/dnscache.h` and `.cpp` - IP-to-hostname cache. Store multiple candidates per IP. TTL-aware expiry with configurable stale window. `lookup(remoteIp, optionalPid, flowStartTime)` returns best candidate with confidence. Evict after stale window.
- [x] 5.3 Add `advapi32.lib` and `tdh.lib` to `login.pro` LIBS.
- [x] 5.4 Wire DnsObservation into DnsCache and FlowManager. Implement hostname attribution priority: IP+PID+recent+TTL valid (high) > IP+recent (medium) > IP+stale (low) > reverse DNS disabled by default (low) > unknown (none).
- [x] 5.5 Add Phase 5 source/header files to `login.pro`.
- [x] 5.6 Build and verify. Confirm network sessions show hostname candidates with correct confidence.

## Phase 6: ProtocolInferencer

- [x] 6.1 Create `network/protocol/protocolinferencer.h` and `.cpp` - stateless port-based inference. TCP/443 -> HTTPS (medium), TCP/80 -> HTTP (medium), UDP/443 -> QUIC (medium-low), TCP+UDP/53 -> DNS (high), TCP/853 -> DoT (medium), TCP/22 -> SSH (medium), etc. Each hint includes confidence and reason string.
- [x] 6.2 Call ProtocolInferencer inline from FlowManager when closing a session (stateless, no separate thread needed).
- [x] 6.3 Add Phase 6 source/header files to `login.pro`.
- [x] 6.4 Build and verify. Confirm `app_protocol_hint`, `app_protocol_confidence`, `app_protocol_reason` appear in JSONL records.

## Phase 7: Integration with MainWindow

- [x] 7.1 Create `network/orchestrator/networkorchestrator.h` and `.cpp` - owns all subsystem objects. `start(username)` prepares log directory, starts writer -> FlowManager -> DNS -> IP Helper -> captures (in order). `stop()` stops captures first -> DNS/IP Helper -> flush FlowManager -> stop writer -> join threads. Emits `statusChanged(QString)`.
- [x] 7.2 Add `NetworkOrchestrator *m_networkOrch` to `mainwindow.h` with forward declaration.
- [x] 7.3 In `MainWindow::onLogin()` - call `m_networkOrch->start(result.username)` after starting ActivityLogger.
- [x] 7.4 In `MainWindow::onLogout()` - call `m_networkOrch->stop()` before stopping ActivityLogger and hooks.
- [x] 7.5 In `MainWindow::closeEvent()` - call `m_networkOrch->stop()` before existing cleanup.
- [x] 7.6 Display `statusChanged` text in the MainWindow status bar (status bar text only, no separate panel).
- [x] 7.7 Add all remaining network source/header files to `login.pro`.
- [x] 7.8 Full build and integration test - login, browse websites, verify `network_log.jsonl` populates with session records. Verify `activity_log` is unchanged. Verify logout flushes all sessions.

## Phase 8: Hardening

- [x] 8.1 Verify admin manifest works - UAC prompt appears on launch. Clicking "No" prevents the app from starting.
- [x] 8.2 Test runtime admin check fallback - if manifest is stripped, app shows error dialog and exits.
- [x] 8.3 Test missing WinDivert DLL/SYS - app starts in degraded mode, logs error, no crash.
- [x] 8.4 Test ETW session failure - sessions get `hostname: null`, no crash.
- [x] 8.5 Test IP Helper API failure - sessions get `process: unknown`, no crash.
- [x] 8.6 Test shutdown while `WinDivertRecv` is blocked - `WinDivertClose` unblocks, clean exit.
- [x] 8.7 Test high-volume traffic - verify no UI freezes, FlowManager handles backpressure.
- [x] 8.8 Test PID reuse - ProcessResolver validates creation time, no false attribution.
- [x] 8.9 Verify existing ActivityLogger and hooks still work correctly after all changes.
- [x] 8.10 Final full build with no warnings. Verify all JSONL records match schema_version 1.

## Phase 9: Follow-up Runtime Log Refinements

- [x] 9.1 Add `application_layer_category` to the compact JSONL schema.
- [x] 9.2 Improve DNS ETW parsing fallback for `QueryResults` payloads that expose IP answers only in raw event data.
- [x] 9.3 Add FlowManager tests for application-layer category output.
