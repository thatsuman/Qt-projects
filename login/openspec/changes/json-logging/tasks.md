## 1. Include JSON Headers

- [x] 1.1 Include `<QJsonObject>` and `<QJsonDocument>` headers in `logger/activitylogger.cpp`.

## 2. Implement JSON Logging Formats

- [x] 2.1 Update `ActivityLogger::start` to append the `session_start` event in JSON Lines format to the new `.jsonl` log file.
- [x] 2.2 Update `ActivityLogger::writeEntry` to serialize activity log fields into a `QJsonObject`.
- [x] 2.3 Convert `QJsonObject` to a compact single-line string using `QJsonDocument` and append it to `activity_log_<username>.jsonl`.
- [x] 2.4 Confirm that the plain text error logging in `activity_error.txt` remains unchanged.

## 3. Verification & Testing

- [x] 3.1 Perform a clean build of the application and verify it compiles without errors or warnings.
- [x] 3.2 Launch the application, login, perform foreground window switching, keyboard activity, and mouse movements, then logout.
- [x] 3.3 Verify that the generated `activity_log_<username>.jsonl` contains valid, newline-separated JSON objects matching the defined schema.
