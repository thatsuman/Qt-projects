# AI Agent Prompt: Implement Hybrid Network Logging Module

You are coding inside an existing Windows Qt Widgets C++17 desktop application that currently logs user activity. The developer uses VS Code, Codex, OpenSpec, qmake, MSVC, and Qt 5.15.x.

## Current repository state

The current app is intentionally back to a non-network-monitoring state. It contains:
- AuthManager for credential checks
- KeyboardHook and MouseHook for input capture
- ActivityLogger for JSONL foreground/input activity sessions
- LoginUIManager for login/logout UI state
- MainWindow as the orchestrator

The previous ETW network module was removed. Do not resurrect the old ETW implementation. Treat it only as historical context.

## Goal

Implement a new separate network logging module using a hybrid approach:
- WinDivert for packet visibility and byte accounting
- ETW DNS Client events for hostname/domain correlation
- IP Helper API for process ownership fallback and snapshots

The module must produce useful network session records such as:
- local endpoint
- remote endpoint
- remote domain/hostname when known
- process PID/name/path when known
- transport protocol
- application protocol hint, not exact L7 truth
- bytes sent/received
- packet counts
- start/end time
- close reason
- confidence/source fields for hostname and process attribution

## Important non-goals

Do not implement browser history capture.
Do not attempt TLS decryption or MITM proxying.
Do not log HTTP bodies, request payloads, response bodies, credentials, cookies, tokens, or decrypted content.
Do not infer exact REST/gRPC/WebSocket semantics unless confidently visible. For encrypted traffic, only emit protocol hints such as HTTPS, QUIC, DNS, TCP, UDP.
Do not mix network logging into ActivityLogger.
Do not put capture/parsing/writing logic inside MainWindow.
Do not block, modify, drop, or reinject packets for this logging feature.

## Architecture requirement

Keep the network system separate from current activity logging.

MainWindow should only orchestrate lifecycle:
- on successful login: start ActivityLogger and start NetworkOrchestrator
- on logout/close: stop NetworkOrchestrator safely, then stop existing modules as appropriate

Recommended module tree:

network/
  model/
    networkevents.h
    flowkey.h
    flowsession.h
    networksessionrecord.h
  orchestrator/
    networkorchestrator.h
    networkorchestrator.cpp
  capture/
    windivertpacketcapture.h
    windivertpacketcapture.cpp
    windivertflowcapture.h
    windivertflowcapture.cpp
  flow/
    flowmanager.h
    flowmanager.cpp
  dns/
    etwdnsmonitor.h
    etwdnsmonitor.cpp
    dnscache.h
    dnscache.cpp
  process/
    processresolver.h
    processresolver.cpp
    iphelperconnectionpoller.h
    iphelperconnectionpoller.cpp
  protocol/
    protocolinferencer.h
    protocolinferencer.cpp
  writer/
    networkjsonlwriter.h
    networkjsonlwriter.cpp

Keep names close to these unless the repo already has a naming convention.

## FlowManager design - implement first

FlowManager is the core brain. It converts low-level events into network sessions.

Responsibilities:
- Own the in-memory map of active flows.
- Normalize packet endpoints into a stable FlowKey.
- Create/update FlowSession objects from packet observations.
- Track bytes sent, bytes received, packets sent, packets received.
- Track firstSeenUtc, lastSeenUtc, direction, TCP flags, close reason.
- Enrich flows with process information from WinDivert FLOW/SOCKET events and IP Helper snapshots.
- Enrich flows with hostname/domain information from DnsCache.
- Emit final NetworkSessionRecord objects when flows close or time out.
- Flush all active flows on logout/app shutdown.
- Provide throttled snapshots for optional UI display.

FlowManager must not:
- open WinDivert handles
- start ETW sessions
- call Windows APIs directly for process enumeration
- write files directly
- update Qt UI directly

### Flow input events

Define small data objects for internal communication:

PacketObservation:
- timestampUtc
- ipVersion
- direction: inbound/outbound
- transport: TCP/UDP/ICMP/Other
- srcIp, dstIp
- srcPort, dstPort, if available
- packetBytes
- payloadBytes, if cheaply available
- tcpFlags: syn, ack, fin, rst, if TCP
- loopback flag
- interface index, if available

FlowLifecycleObservation:
- timestampUtc
- event: established/deleted
- pid, if available
- localIp, localPort
- remoteIp, remotePort
- transport
- source: windivert_flow or windivert_socket

ProcessConnectionSnapshot:
- timestampUtc
- pid
- processName
- processPath, if available
- localIp, localPort
- remoteIp, remotePort
- transport
- state, for TCP
- source: iphelper

DnsObservation:
- timestampUtc
- queryName
- answerIps
- ttl, if available
- pid, if available
- processName, if available
- status/result code, if available
- source: etw_dns_client

### FlowKey normalization

Do not use domain or process name as part of the key.

Use:
- localIp
- localPort
- remoteIp
- remotePort
- transport
- ipVersion

For outbound packets:
- local = src
- remote = dst

For inbound packets:
- local = dst
- remote = src

For TCP, this key is stable for a connection.
For UDP, treat it as a pseudo-session and close by idle timeout.

### FlowSession fields

Each active session should track:
- flowId: stable UUID or hash-derived ID
- FlowKey
- startTimeUtc
- lastSeenUtc
- directionFirstSeen
- bytesSentTotal
- bytesReceivedTotal
- payloadBytesSent
- payloadBytesReceived
- packetsSent
- packetsReceived
- tcpSynSeen
- tcpFinSeen
- tcpRstSeen
- closeReason
- process.pid
- process.name
- process.path
- process.source
- process.confidence
- remoteHost.primaryName
- remoteHost.candidates
- remoteHost.source
- remoteHost.confidence
- appProtocolHint
- appProtocolConfidence
- appProtocolReason
- flags: ipv6, loopback

### Flow close policy

Close and emit final session records when:
- TCP RST is seen
- TCP FIN is seen and no further packets arrive after a short grace window
- WinDivert flow deletion event arrives
- TCP flow is idle for a configured timeout, for example 120 seconds
- UDP flow is idle for a configured timeout, for example 30 seconds
- app logout or app shutdown occurs

Use timers inside the network worker thread. Do not block the UI thread.

### Enrichment policy

Process attribution priority:
1. WinDivert FLOW/SOCKET event PID for the same tuple, if available
2. IP Helper TCP/UDP owner PID snapshot
3. DNS event PID only as weak hint, only when tuple/process timing supports it
4. Unknown

Hostname attribution priority:
1. ETW DNS result with answer IP matching remote IP, same PID/process if available, timestamp before or near flow start, and TTL not expired
2. ETW DNS result with matching IP and recent timestamp, no PID match
3. Optional reverse DNS fallback, disabled by default or clearly marked low confidence
4. Unknown

Keep multiple domain candidates per IP. Do not overwrite with a single global IP-to-domain value because CDNs and shared hosting can map many names to the same IP.

Protocol inference policy:
- Always record transport truth: TCP, UDP, ICMP.
- Emit appProtocolHint separately.
- Examples:
  - TCP/80 -> HTTP, confidence medium
  - TCP/443 -> HTTPS, confidence medium
  - UDP/443 -> QUIC/HTTP3, confidence medium-low
  - UDP/TCP 53 -> DNS, confidence high
  - TCP/853 -> DNS-over-TLS, confidence medium
- Do not claim REST, GraphQL, gRPC, or WebSocket for encrypted traffic unless the data is actually visible and parsed.

## Capture design

WinDivertPacketCapture:
- Use WinDivert NETWORK layer for packet observations.
- Open in sniff/read-only style, not modifying traffic.
- Use narrow filters to avoid unnecessary overhead, for example TCP/UDP only initially.
- Parse headers only. Do not retain payloads.
- Convert raw packets into PacketObservation and push to FlowManager.
- If packet capture fails, emit a module error and let the rest of the app continue.

WinDivertFlowCapture:
- Use WinDivert FLOW layer if available.
- Capture flow established/deleted events.
- Use it primarily for PID and lifecycle help.
- Push FlowLifecycleObservation to FlowManager.

ETWDnsMonitor:
- Start a real-time ETW session for DNS Client events.
- Parse query name, query result, answer IPs, PID when available, and timestamp.
- Push DnsObservation to DnsCache and FlowManager.
- If ETW DNS fails, the system should still log IP-level sessions with hostname unknown.

IpHelperConnectionPoller:
- Periodically call GetExtendedTcpTable and GetExtendedUdpTable owner-PID variants.
- Build ProcessConnectionSnapshot events.
- Poll faster at first, for example 1 second, then tune.
- Cache PID to process name/path in ProcessResolver.
- Handle short-lived process and PID reuse carefully.

NetworkJsonlWriter:
- Write JSONL records to logs/<username>/network_log.jsonl.
- Also write diagnostic errors to logs/<username>/network_error.txt.
- Never write from capture callbacks directly; use a queue or queued signal.
- Flush on logout/stop.

## Threading model

Use a dedicated network worker thread or a small set of worker threads.

Preferred simple model:
- NetworkOrchestrator lives in the UI/main thread but owns/starts worker objects.
- Packet capture, flow capture, DNS monitor, IP Helper poller, FlowManager, and writer run outside the UI thread.
- FlowManager should be single-owner-threaded. All event sources send events via Qt queued connections or a thread-safe queue drained by FlowManager.
- UI receives only throttled status/snapshot updates, never raw packets.

Do not use locks everywhere if a single FlowManager thread can serialize state updates.

## Runtime lifecycle

On login:
1. MainWindow authenticates user using existing AuthManager.
2. Existing hooks and ActivityLogger start as they do now.
3. MainWindow calls NetworkOrchestrator::start(username).
4. NetworkOrchestrator prepares logs/<username>/.
5. Start writer.
6. Start FlowManager timer.
7. Start DNS cache/monitor.
8. Start IP Helper poller.
9. Start WinDivert packet and flow captures.
10. Emit status: network logging active or degraded.

On logout/close:
1. MainWindow calls NetworkOrchestrator::stop().
2. Stop capture sources first so no new events arrive.
3. Stop ETW DNS session and IP Helper poller.
4. Ask FlowManager to flush all active sessions with closeReason = logout/app_shutdown.
5. Flush and close NetworkJsonlWriter.
6. Join worker threads cleanly.
7. Emit stopped status.

## JSONL schemas

Network session record:

{
  "type": "network_session",
  "schema_version": 1,
  "username": "demo_user",
  "flow_id": "...",
  "start_time_utc": "2026-07-21T10:00:00.000Z",
  "end_time_utc": "2026-07-21T10:00:15.000Z",
  "duration_ms": 15000,
  "local": {
    "ip": "192.168.1.10",
    "port": 53124
  },
  "remote": {
    "ip": "142.250.183.14",
    "port": 443,
    "hostname": "example.com",
    "hostname_source": "etw_dns",
    "hostname_confidence": "medium",
    "hostname_candidates": ["example.com"]
  },
  "process": {
    "pid": 1234,
    "name": "chrome.exe",
    "path": "C:/Program Files/Google/Chrome/Application/chrome.exe",
    "source": "windivert_flow",
    "confidence": "high"
  },
  "transport_protocol": "TCP",
  "app_protocol_hint": "HTTPS",
  "app_protocol_confidence": "medium",
  "app_protocol_reason": "remote_port_443_tcp",
  "bytes": {
    "sent_total": 12345,
    "received_total": 67890,
    "sent_payload": 10000,
    "received_payload": 64000
  },
  "packets": {
    "sent": 40,
    "received": 55
  },
  "close_reason": "idle_timeout",
  "flags": {
    "ipv6": false,
    "loopback": false
  }
}

Optional DNS event record for debugging:

{
  "type": "dns_query",
  "schema_version": 1,
  "timestamp_utc": "2026-07-21T10:00:00.000Z",
  "query_name": "example.com",
  "answer_ips": ["142.250.183.14"],
  "pid": 1234,
  "process_name": "chrome.exe",
  "status": "success",
  "source": "etw_dns_client"
}

## OpenSpec workflow

Before coding, create an OpenSpec change, for example:
- openspec/changes/add-hybrid-network-logging/proposal.md
- openspec/changes/add-hybrid-network-logging/tasks.md
- openspec/changes/add-hybrid-network-logging/specs/network-logging/spec.md

The spec should define requirements for:
- separate network module lifecycle
- packet-to-flow aggregation
- DNS hostname correlation
- process attribution
- JSONL output
- degraded-mode behavior
- shutdown flushing

Do not modify the existing activity-logging spec except where it references shared lifecycle, if necessary.

## Incremental implementation phases

Phase 0: OpenSpec only
- Add proposal, tasks, and spec.
- No runtime code changes.

Phase 1: Pure model and FlowManager tests/fake events
- Add FlowKey, FlowSession, NetworkSessionRecord, PacketObservation.
- Implement FlowManager using fake packet observations.
- Verify JSON session output using synthetic events.

Phase 2: NetworkJsonlWriter
- Write network_log.jsonl under logs/<username>/.
- Add network_error.txt diagnostics.
- Verify writer flushes on stop.

Phase 3: WinDivert packet capture
- Add WinDivert dependency to qmake carefully.
- Capture TCP/UDP packet headers in sniff/read-only mode.
- Convert to PacketObservation.
- Do not process payloads.
- Do not modify/reinject/drop packets.

Phase 4: Flow lifecycle and PID enrichment
- Add WinDivert FLOW or SOCKET capture where appropriate.
- Add ProcessResolver and IP Helper poller.
- Enrich sessions with pid/process.

Phase 5: ETW DNS and DNS cache
- Add ETWDnsMonitor.
- Parse DNS query/result events.
- Implement DnsCache candidate correlation.
- Enrich sessions with hostname/domain and confidence/source.

Phase 6: ProtocolInferencer
- Add port-based and transport-based appProtocolHint.
- Keep confidence and reason fields.

Phase 7: Integration with MainWindow
- Add NetworkOrchestrator member only.
- Start on login, stop on logout and close.
- Keep ActivityLogger untouched.
- Show optional status only, not raw traffic.

Phase 8: Hardening
- Handle missing admin rights.
- Handle missing WinDivert driver/dll/sys.
- Handle ETW session start failure.
- Handle IP Helper failure.
- Handle shutdown while capture thread is blocked.
- Handle high-volume traffic without UI freezes.

## Acceptance criteria

- Existing app still builds and activity logging still works.
- ActivityLogger code is not used for network sessions.
- MainWindow contains only lifecycle orchestration for network logging.
- Network module can be disabled or fail gracefully without killing the app.
- network_log.jsonl is created only after login.
- On logout, all active network sessions are flushed.
- Captured packet data is aggregated into sessions, not logged packet-by-packet by default.
- JSON records include bytes sent/received, endpoints, protocol hint, hostname when known, process when known, and confidence/source fields.
- No browser history, URLs, decrypted payloads, cookies, request bodies, or response bodies are logged.
- All long-running work stays off the UI thread.

## Build notes

Use qmake/MSVC conventions already present in login.pro.
Add Windows libraries only where needed, likely iphlpapi and tdh/advapi32 for ETW depending on implementation.
Do not re-add removed old ETW/network files unless intentionally creating new files with clean names and clean design.

## Coding style

Prefer small classes with single responsibility.
Prefer RAII wrappers for WinDivert handles and ETW sessions.
Use explicit start/stop state and idempotent stop methods.
Log every Windows API failure with GetLastError or HRESULT details.
Keep schema versioning in JSONL from day one.
Use QString/QDateTime/QJsonObject where consistent with the existing project.
