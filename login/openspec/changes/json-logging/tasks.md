## 1. Include JSON Headers

- [x] 1.1 Include `<QJsonObject>` and `<QJsonDocument>` headers in `logger/activitylogger.cpp`.

## 2. Implement JSON Logging Formats

- [x] 2.1 Update `ActivityLogger::start` to append the `session_start` event in JSON Lines format to the new `.jsonl` log file.
- [x] 2.2 Update `ActivityLogger::writeEntry` to serialize activity log fields into a `QJsonObject`.
- [x] 2.3 Convert `QJsonObject` to a compact single-line string using `QJsonDocument` and append it to `activity_log_<username>.jsonl`.
- [x] 2.4 Confirm that the plain text error logging in `activity_error.txt` remains unchanged.
- [x] 2.5 Update `ActivityLogger::onTimer` to flush previous activity ONLY when the foreground process (`processName`) changes, rather than when the window title changes.
- [x] 2.6 In `ActivityLogger::onTimer`, if the process remains the same but the window title changes, update `m_lastWindowTitle` to the new title without flushing.

## 3. Verification & Testing

- [x] 3.1 Perform a clean build of the application and verify it compiles without errors or warnings.
- [x] 3.2 Launch the application, login, perform window switching within the same application (e.g., browser tabs), and verify it doesn't write separate objects.
- [x] 3.3 Switch focus to a different application (e.g., Notepad), and verify that it flushes the previous app's entry, creating separate objects with accumulated keystrokes and mouse distance.

## 4. Implement Network Logger Module

- [x] 4.1 Create `logger/networklogger.h` defining the `NetworkLogger` class with connection polling and ETW trace thread handles.
- [x] 4.2 Create `logger/networklogger.cpp` implementing IP Helper API table polling (`GetExtendedTcpTable`/`GetExtendedUdpTable`) and socket-to-PID lookup.
- [x] 4.3 Implement Windows DNS Client ETW trace callback parser to construct a local IP-to-Domain mapping cache.
- [x] 4.4 Implement `network_log_<username>.jsonl` JSON Lines file output serialization.
- [x] 4.5 Link Windows `iphlpapi.lib` and `tdh.lib` libraries in `login.pro`.
- [x] 4.6 Instantiate and wire `NetworkLogger` start/stop routines in `MainWindow`.

## 5. Network Monitoring Verification

- [x] 5.1 Rebuild the project and verify it compiles without errors or warnings.
- [x] 5.2 Launch the application, browse some websites in a browser, run networking commands in terminal, and verify that `network_log_<username>.jsonl` gets populated with correct PID-resolved process names and domain hostnames.
- [x] 5.3 Verify that activity logs and network logs are completely separated into their respective files.

## 6. Network Logger Improvements

- [x] 6.1 Refactor `NetworkLogger` to group connections by PID: accumulate connections into `m_pidSessions` map, flush each PID session when the process exits or logging stops.
- [x] 6.2 Move all log files into a per-user directory `logs/<username>/`. Create directory with `QDir::mkpath` on session start in both `ActivityLogger` and `NetworkLogger`.
- [x] 6.3 Fix ETW buffer flushing: set `FlushTimer = 1` on `EVENT_TRACE_PROPERTIES` before `StartTrace` so DNS events are delivered within 1 second instead of waiting for buffer to fill.
- [x] 6.4 Add `QHostInfo` async reverse-DNS fallback in `logConnection`: when ETW cache returns empty domain, call `QHostInfo::lookupHost` for the remote IP and update the log record with the resolved hostname.
- [x] 6.5 Rebuild, deploy DLLs, run as Administrator and verify `network_log.jsonl` shows grouped connections per PID with populated domain/hostname fields.

