## Why

The application currently logs user activity (foreground windows, keystrokes, mouse distance) but has no visibility into network traffic. A previous ETW-only network module was removed due to design issues—it mixed concerns, ran in the wrong threads, and lacked proper flow-level aggregation.

We need a new, cleanly-separated network logging module that captures **flow-level network sessions** (not raw packets) with rich metadata: endpoints, byte counts, process attribution, hostname correlation, and application protocol hints. This requires a hybrid approach combining WinDivert for packet visibility, ETW DNS Client events for hostname resolution, and IP Helper API for process ownership fallback.

Additionally, the application must require **administrator privileges** at startup. This is mandatory because WinDivert needs elevation to load its kernel driver, and real-time ETW sessions require admin to start a trace. The app should prompt via UAC before launching, and exit if the user declines.

## What Changes

- Add a mandatory admin privilege requirement via Windows application manifest and runtime safety check.
- Implement a new `network/` module tree, fully separate from the existing `logger/` (ActivityLogger).
- Introduce WinDivert 2.2.2 (vendored in `third_party/`) for passive packet capture (SNIFF + RECV_ONLY mode—no traffic modification).
- Implement FlowManager as the core aggregation engine: converts raw packet observations into network session records.
- Add ETW DNS Client monitoring for hostname-to-IP correlation via DnsCache.
- Add IP Helper API polling for process-to-connection attribution as a fallback.
- Add ProtocolInferencer for port-based application protocol hinting.
- Write completed sessions to `logs/<username>/network_log.jsonl` in JSONL format with schema versioning.
- Integrate NetworkOrchestrator lifecycle into MainWindow (start on login, stop on logout/close).
- All long-running work runs off the UI thread on dedicated worker threads.

## Capabilities

### New Capabilities

- `network-logging`: Captures TCP/UDP traffic passively via WinDivert, aggregates packets into flow-level session records enriched with process attribution (WinDivert FLOW + IP Helper), hostname correlation (ETW DNS Client + DnsCache), and port-based protocol hints. Writes JSONL records with byte counts, packet counts, endpoints, confidence fields, and close reasons. Each subsystem degrades gracefully on failure.
- `admin-elevation`: Requires administrator privileges at startup via embedded Windows manifest (`requireAdministrator`). Includes runtime fallback check that shows an error dialog and exits if elevation is missing.

### Modified Capabilities

- `activity-logging`: No functional changes. MainWindow lifecycle is extended to also start/stop the NetworkOrchestrator alongside ActivityLogger.

## Impact

- **New Files:** ~30 files across `network/` module tree (model, flow, capture, dns, process, protocol, writer, orchestrator), plus `login.manifest`, `login.rc`, and a `test_flowmanager` test executable.
- **Modified Files:** `main.cpp` (admin check), `mainwindow.h` / `mainwindow.cpp` (NetworkOrchestrator member + lifecycle), `login.pro` (new sources, headers, libs, manifest).
- **Dependencies:** WinDivert 2.2.2 (vendored, LGPL—license file included), `iphlpapi.lib`, `advapi32.lib`, `tdh.lib`. No `QT += network`—uses custom `IpAddress` type.
- **Output Files:**
  * Network session logs: `logs/<username>/network_log.jsonl`
  * Network error diagnostics: `logs/<username>/network_error.txt`
  * Activity logs unchanged: `logs/<username>/activity_log_<username>.jsonl`
