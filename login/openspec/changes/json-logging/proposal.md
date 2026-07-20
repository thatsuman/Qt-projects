## Why

The current logging implementation writes user activity records in a custom plain text format separated by pipe (|) and hyphen (-) characters. This format is hard to parse programmatically, does not scale well when integrating with external log collectors (like Elasticsearch, Logstash, or Fluentd), and is difficult to extend with new event types such as network traffic logs. Converting logs to JSON Lines (JSONL) solves these issues by providing a structured, standard, and highly extensible output format.

## What Changes

- Modify user activity logs to use the JSON Lines (JSONL) format instead of custom plain text.
- Change the log file suffix from `.txt` to `.jsonl` (e.g., `activity_log_<username>.jsonl`).
- Introduce structured event types (`session_start` and `activity`) to replace freeform text headers and plain text lines.
- Ensure exception/error logging in `activity_error.txt` remains plain text as-is.

## Capabilities

### New Capabilities

- `activity-logging`: Captures foreground window changes, keystrokes, and mouse distance, serializing them into structured JSON Lines format.

### Modified Capabilities

*None (no existing specs in openspec/specs)*

## Impact

- **Affected Code:** `ActivityLogger` module (`logger/activitylogger.h`, `logger/activitylogger.cpp`).
- **Dependencies:** Qt's JSON modules (`QJsonObject`, `QJsonDocument`).
- **Output Files:** Activity log files will be output as `.jsonl` instead of `.txt`.
