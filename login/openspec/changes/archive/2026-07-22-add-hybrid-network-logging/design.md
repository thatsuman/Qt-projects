## Context

The application is a Windows Qt Widgets C++17 desktop app that logs user activity. It currently has AuthManager, KeyboardHook, MouseHook, ActivityLogger, LoginUIManager, and MainWindow. A previous ETW-only network module was removed. This design covers a new hybrid network logging system built from scratch, plus mandatory admin elevation.

The design follows an 8-phase incremental implementation plan where each phase can be built and validated independently.

## Goals / Non-Goals

**Goals:**
- Passive packet capture using WinDivert 2.2.2 in SNIFF + RECV_ONLY mode (no traffic modification/reinjection).
- Flow-level session aggregation in FlowManager with idle timeout, TCP state tracking, and flush-on-shutdown.
- Process attribution via WinDivert FLOW/SOCKET events (high confidence) and IP Helper polling (medium confidence).
- Hostname correlation via ETW DNS Client events and a TTL-aware DnsCache with stale window support.
- Port-based application protocol hinting (HTTPS, DNS, SSH, etc.) with confidence and reason fields.
- JSONL output with schema versioning (`schema_version: 1`), written to `logs/<username>/network_log.jsonl`.
- Mandatory admin privilege via Windows application manifest with runtime fallback.
- Each subsystem degrades gracefully—a single failure doesn't kill the app.
- All blocking I/O and heavy computation off the UI thread.

**Non-Goals:**
- Browser history capture, TLS decryption, MITM proxying, or payload inspection.
- Logging HTTP bodies, request payloads, response bodies, credentials, cookies, or tokens.
- Inferring exact REST/gRPC/WebSocket semantics for encrypted traffic.
- Modifying, dropping, or reinjecting network packets.
- Building a network traffic UI viewer or dashboard (status bar text only for Phase 7).
- Modifying ActivityLogger code or format.

## Decisions

### 1. WinDivert 2.2.2 for Packet Visibility

- **Choice:** WinDivert NETWORK layer with `WINDIVERT_FLAG_SNIFF | WINDIVERT_FLAG_RECV_ONLY`.
- **Rationale:** SNIFF mode observes packets without diverting them from the network stack. RECV_ONLY means we only receive—never reinject. This combination is purely passive. WinDivert provides header-level packet data (IP/TCP/UDP) sufficient for flow tracking without payload inspection.
- **Vendored:** Pinned to version 2.2.2 in `third_party/WinDivert/` with LGPL license file included. The DLL and signed .sys driver must be beside the built .exe at runtime.
- **Alternative considered:** Raw sockets, Npcap/WinPcap. Rejected: raw sockets on Windows are restricted; Npcap requires a separate driver install and has licensing constraints.

### 2. Hybrid Process Attribution (WinDivert FLOW + IP Helper)

- **Choice:** WinDivert FLOW layer for PID enrichment (high confidence), IP Helper `GetExtendedTcpTable`/`GetExtendedUdpTable` for fallback (medium confidence).
- **Rationale:** WinDivert FLOW provides PID at connection establishment/teardown. IP Helper provides a snapshot of all active connections with owner PIDs, catching flows that WinDivert FLOW missed. Together they cover most attribution scenarios.
- **Note:** IP Helper does NOT require admin privileges on most Windows versions. Admin is justified by WinDivert and ETW, not IP Helper.

### 3. ETW DNS Client Correlation (Generic, Not Hard-Coded to Event IDs)

- **Choice:** Real-time ETW session on `Microsoft-Windows-DNS-Client` provider (`{1C95126E-7EEA-49A9-A3FE-A378B03DDB4D}`).
- **Spec level:** Require DNS Client ETW correlation to extract query name, answer IPs, PID, and timestamp. No specific Event IDs are mandated.
- **Design level:** Start with known event IDs (e.g., 3008 for query completion) as initial parsing targets. MUST log unrecognized event IDs and their provider schema/properties to `network_error.txt` so parsers can be added later. Use TDH (Trace Data Helper) to dynamically enumerate event properties when possible.
- **Alternative considered:** Hooking `DnsQuery_A/W`. Rejected: fragile, version-dependent, and misses system-level queries.

### 4. DnsCache with Stale Window

- **Choice:** IP-to-hostname cache with TTL-aware expiry plus a configurable stale window.
- **Rationale:** DNS entries remain useful briefly past TTL—connections established just before TTL expiry still reference them. Keeping entries in a stale window with reduced confidence avoids losing hostname attribution for active flows.
- **Policy:** Multiple hostnames per IP (CDN/shared hosting). PID matching boosts confidence. Entries evicted after stale window expires.

### 5. Custom IpAddress Type Instead of QHostAddress

- **Choice:** Lightweight `IpAddress` type wrapping `std::array<uint8_t, 16>` (128 bits, stores both IPv4 and IPv6).
- **Rationale:** Avoids adding `QT += network` to the project just for `QHostAddress`. The custom type provides `toString()`, `fromV4(quint32)`, `fromV6(const uint8_t*)`, `operator==`, and a `qHash()` overload—all that's needed for flow keys and display.
- **Alternative considered:** `QHostAddress` from QtNetwork. Rejected: pulling in the entire QtNetwork module for a single address type is unnecessary overhead.

### 6. Thread Architecture (5-6 Worker Threads)

- **Choice:** Each blocking subsystem gets its own dedicated QThread. FlowManager runs on its own thread with an event loop and timer. No subsystem shares a thread with FlowManager.
- **Threads:**
  1. WinDivert packet capture (blocking `WinDivertRecv` loop)
  2. WinDivert flow capture (blocking `WinDivertRecv` loop)
  3. FlowManager (event loop + idle-sweep `QTimer`)
  4. ETW DNS monitor (blocking `ProcessTrace` call)
  5. IP Helper connection poller (event loop + polling `QTimer`)
  6. NetworkJsonlWriter (event loop, queued writes)
- **Rationale:** `ProcessTrace` blocks until the session stops—it MUST NOT run in the FlowManager thread or it would starve event processing. IP Helper polling is timer-based and needs its own event loop. All data flows between threads via Qt queued signal-slot connections—no explicit locks on the flow map.

### 7. Mandatory Admin via Manifest + Runtime Fallback

- **Choice:** Embed a `requireAdministrator` manifest in the .exe (Layer 1). Add a runtime `CheckTokenMembership` check in `main.cpp` (Layer 2).
- **Rationale:** The manifest ensures Windows shows the UAC prompt before the process starts. If the user clicks "No," the app never launches. The runtime check catches edge cases where the manifest is stripped or compatibility mode overrides it. Per-subsystem error handling (Layer 3) catches individual access-denied failures gracefully.

### 8. Separate Test Executable for FlowManager

- **Choice:** Create a standalone `test_flowmanager` executable that feeds synthetic PacketObservation events and validates emitted NetworkSessionRecords.
- **Rationale:** Avoids polluting the production app with test data. Allows rapid iteration on FlowManager logic without launching the full app.

## Risks / Trade-offs

- **Risk:** WinDivert driver/DLL missing or blocked by enterprise security policies.
  - **Mitigation:** Graceful degradation—log to `network_error.txt`, continue without packet capture. IP Helper + ETW still provide partial data.
- **Risk:** ETW session start fails (permissions, session limit reached).
  - **Mitigation:** Sessions get `hostname: null`. The rest of the system continues.
- **Risk:** High-volume traffic overwhelms FlowManager.
  - **Mitigation:** FlowManager throttles UI snapshots. Writer uses buffered I/O. Packet capture drops are logged, not fatal.
- **Risk:** PID reuse causes misattribution.
  - **Mitigation:** ProcessResolver uses short cache TTL and validates PID creation time against flow start time.
- **Risk:** WinDivert LGPL license requires careful handling.
  - **Mitigation:** Vendor as a dynamic library (DLL), include license file, do not statically link.
- **Risk:** `WinDivertRecv` blocks indefinitely on shutdown.
  - **Mitigation:** `WinDivertClose()` from the stop thread unblocks the recv call, which returns an error. The capture loop detects this and exits.
