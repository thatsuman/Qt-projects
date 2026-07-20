## Context

The `ActivityLogger` module writes user activity events to user-specific text logs on a periodic timer. Currently, this data is formatted using custom text delimiters, which is hard to parse and scale. We are migrating the serialization format to JSON Lines (JSONL).

## Goals / Non-Goals

**Goals:**
- Implement JSON Lines (JSONL) serialization for session start and activity events.
- Change log file names to `activity_log_<username>.jsonl`.
- Keep memory usage and write performance optimal by using append-only, compact JSON serialization.
- Maintain existing plain-text error logging in `activity_error.txt` without changes.

**Non-Goals:**
- Creating any log viewer or parsing functionality in the application UI.
- Modifying the underlying Windows Hooks or the window-tracking polling interval.
- Modifying the existing authentication model or login logic.

## Decisions

### 1. JSON Lines (JSONL) format instead of Single JSON Array
- **Choice:** JSON Lines (newline-separated JSON objects).
- **Rationale:** Appending a line is an $O(1)$ disk operation. It does not require parsing the entire log file in memory before adding an entry, making it fast and preventing memory spikes on systems with large log histories.
- **Alternative considered:** Standard JSON array. Rejected because of $O(N)$ write overhead and high risk of file corruption if the app crashes during rewriting.

### 2. Qt Core JSON Classes
- **Choice:** Use `QJsonObject` and `QJsonDocument`.
- **Rationale:** Built-in classes in Qt 5 and 6 that compile cross-platform, are well-tested, and easily convert maps of key-value pairs into compact JSON bytes.
- **Alternative considered:** Manual string building. Rejected because manual escaping of string values (like window titles with quotes/backslashes) is error-prone and can break JSON formatting.

## Risks / Trade-offs

- **Risk:** Log files can grow large if the user remains logged in for a long time.
- **Mitigation:** Keep the JSON format compact (`QJsonDocument::Compact`) to minimize whitespace overhead.
- **Risk:** Incomplete JSON lines if a write is interrupted by crash.
- **Mitigation:** Append each line atomically with `\n` using Qt's `QFile` buffers.
