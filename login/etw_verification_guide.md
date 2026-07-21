# ETW Network Protocol Analysis — Verification Guide

## Prerequisites

All verification tasks require:
- **Administrator privileges** (ETW real-time tracing requires elevation)
- Qt build: `Build → Rebuild All` from Qt Creator after opening `login.pro`
- A valid login credential to start the ETW session (any credentials in `AuthManager`)

---

## 5.1 — REST API vs WebSocket Protocol Classification

**Objective:** Verify that `ProtocolClassifier` correctly distinguishes REST API calls from WebSocket connections against live test endpoints.

### Test Procedure

1. Launch the application **as Administrator** (`Run as Administrator`)
2. Login with valid credentials → ETW session starts, telemetry view appears
3. Open a browser and navigate to:
   - `https://jsonplaceholder.typicode.com/todos/1` — REST API GET request
   - `wss://echo.websocket.org` — WebSocket connection
4. Observe the green telemetry panel in the application window

### Expected Output

```
[NEW] <localIP> -> 104.x.x.x | Port: <ephemeral> -> 443 | Protocol: TLS/SSL | ...
[UPGRADE] <localIP> -> 104.x.x.x | Protocol transition: TCP -> REST API | Target: https://jsonplaceholder.typicode.com/todos/1
[NEW] <localIP> -> 174.x.x.x | Port: <ephemeral> -> 443 | Protocol: TLS/SSL | ...
[UPGRADE] <localIP> -> 174.x.x.x | Protocol transition: TCP -> WebSocket | Target: wss://echo.websocket.org
```

### Classification Logic Reference

| Detection Signal | Protocol Assigned |
|---|---|
| `ws://` or `wss://` URL scheme in WebIO event | `WebSocket` |
| `Upgrade: websocket` header in WebIO HTTP event | `WebSocket` |
| HTTP GET/POST + JSON/XML ContentType via WinINet | `REST API` |
| Remote port 443 + TCP-only events | `TLS/SSL` |
| Remote port 53 | `DNS` |
| All other TCP | `RAW_TCP` |

---

## 5.2 — Process Attribution (PID → Process Executable Path)

**Objective:** Validate that each ETW event is correctly attributed to its originating process by PID and executable name.

### Test Procedure

1. Launch application as Administrator and login
2. Open a browser (e.g. `chrome.exe`) and load any HTTPS URL
3. Open a second app (e.g. `curl.exe` via PowerShell: `curl https://httpbin.org/get`)
4. After ~5 seconds (timer flush cycle), check `logs/<username>/network_log.jsonl`

### Expected JSONL Entry

```json
{
  "type": "network_session",
  "pid": 12345,
  "process_name": "chrome.exe",
  "connections": [...]
}
```

### Verification Points

- `process_name` field must match the actual process name (not `"unknown"`)
- PID must correspond to a real running process (verify with Task Manager)
- Multiple processes must each get their own separate `network_session` entry

### Implementation Reference

`NetworkLogger::getProcessNameFromPid()` uses `OpenProcess` + `GetModuleBaseName` (psapi.h).

---

## 5.3 — Consecutive Connection Consolidation & Bandwidth Aggregation

**Objective:** Verify that multiple TCP connections from the same local IP to the same remote IP are consolidated into a single `ConnectionRecord` with summed bandwidth counters.

### Test Procedure

1. Launch application as Administrator and login
2. Load a web page that spawns many parallel connections (e.g. `https://www.google.com`) — browsers typically open 6–15 parallel TCP connections to the same server IP
3. After 5 seconds, check `logs/<username>/network_log.jsonl`

### Expected JSONL Entry (consolidated)

```json
{
  "local_ip": "192.168.x.x",
  "remote_ip": "142.250.x.x",
  "local_ports": [52001, 52002, 52003, 52004],
  "remote_ports": [443],
  "protocol": "TLS/SSL",
  "bytes_sent": 8192,
  "bytes_received": 245760,
  "bytes_sent_formatted": "8.0 KB",
  "bytes_received_formatted": "240.0 KB",
  "connection_count": 4
}
```

### Verification Points

- `local_ports` array must contain multiple ephemeral ports (not single port)
- `connection_count` must be > 1
- `bytes_sent` + `bytes_received` must be cumulative sum across all consolidated events
- Only ONE record per `(localIp, remoteIp)` pair per PID session

### Consolidation Criterion (from `NetworkLogger::onEtwTcpEvent`)

```cpp
if (lastRec.localIp == event.localIp && lastRec.remoteIp == event.remoteIp) {
    // merge: append port, accumulate bytes, increment connectionCount
}
```

---

## 5.4 — Performance Under Sustained Network Throughput

**Objective:** Validate that ETW event processing does not drop events, cause UI freezes, or consume excessive CPU under sustained gigabit-class network activity.

### Test Procedure

1. Launch application as Administrator and login
2. Run a sustained download: `curl -o NUL https://speed.hetzner.de/100MB.bin`
3. Monitor:
   - Application UI responsiveness (no freeze, window remains responsive)
   - CPU usage in Task Manager (ETW thread should stay < 5% CPU)
   - `network_logger_debug.txt` for any timeout/warning messages

### ETW Buffer Configuration

The session is configured with:
```
FlushTimer = 1 second (real-time flushing)
LogFileMode = EVENT_TRACE_REAL_TIME_MODE
```

Per the design: configure `BufferSize = 64KB`, `MinimumBuffers = 16`, `MaximumBuffers = 128` if event drops are observed.

### Performance Expectations

| Metric | Acceptable Threshold |
|---|---|
| UI frame rate | No visible freeze (> 30 fps) |
| ETW thread CPU | < 5% on modern hardware |
| Event drop rate | 0 drops under 100 Mbps |
| Memory (PID sessions map) | < 50 MB for 500 active PIDs |

### Advanced: Enable Buffer Tuning

To add explicit buffer tuning, modify `EtwTraceSession::startSession()`:
```cpp
m_traceProperties->BufferSize    = 64;    // 64 KB per buffer
m_traceProperties->MinimumBuffers = 16;
m_traceProperties->MaximumBuffers = 128;
```

---

## Verification Summary

| Task | Status | Method |
|---|---|---|
| 5.1 REST vs WebSocket classification | Manual runtime test required | Browser to jsonplaceholder + echo.websocket.org |
| 5.2 PID → process attribution | Manual runtime test required | Check `network_log.jsonl` process_name field |
| 5.3 Consolidation & bandwidth aggregation | Manual runtime test required | Load multi-connection page, check JSONL local_ports array |
| 5.4 Performance under throughput | Manual runtime test required | Sustained download + Task Manager monitoring |

> [!IMPORTANT]
> All verification tasks require the application to be run **as Administrator**.
> ETW real-time session creation (`StartTrace`) will silently fail and return
> `false` from `startSession()` without elevation. Check `network_logger_debug.txt`
> if the telemetry panel shows no events.
