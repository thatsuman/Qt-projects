## Why

The current logging implementation only tracks user window focus, keystrokes, and mouse distances. There is a need to capture network connections (protocol, local/remote IPs, ports, processes) to monitor application network traffic simple and driverless. Since remote IPs are often fronted by CDNs, a native DNS client ETW (Event Tracing for Windows) monitoring approach is introduced to capture domain-to-IP mappings, ensuring that the actual domain name visited by browsers/terminals is logged rather than generic CDN hostnames. Network logs will be stored separately from activity logs to prevent log contamination.

## What Changes

- Modify user activity logs to use the JSON Lines (JSONL) format instead of custom plain text (completed).
- Implement a driverless, user-mode `NetworkLogger` using Windows IP Helper API to poll connection tables.
- Implement a background ETW thread to trace `Microsoft-Windows-DNS-Client` events, dynamically caching IP-to-Domain mappings.
- Output network traffic events to a separate file: `network_log_<username>.jsonl`.
- Ensure exception/error logging in `activity_error.txt` remains plain text as-is.

## Capabilities

### New Capabilities

- `activity-logging`: Captures foreground window changes, keystrokes, and mouse distance, serializing them into structured JSON Lines format.
- `network-monitoring`: Tracks active TCP and UDP connections mapped to PIDs, resolves IP hostnames via ETW DNS client interception, and writes them to a separate JSON Lines log.

### Modified Capabilities

*None (no existing specs in openspec/specs)*

## Impact

- **Affected Code:** `ActivityLogger` module (`logger/activitylogger.h`, `logger/activitylogger.cpp`), build configuration (`login.pro`).
- **New Files:** `NetworkLogger` module (`logger/networklogger.h`, `logger/networklogger.cpp`).
- **Dependencies:** Windows IP Helper API (`iphlpapi.lib`, `ws2_32.lib`), Windows Trace Data Helper (`tdh.lib`), Qt Core JSON.
- **Output Files:**
  *   Activity logs: `activity_log_<username>.jsonl`
  *   Network logs: `network_log_<username>.jsonl`
