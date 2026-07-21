## ADDED Requirements

### Requirement: JSON Lines format for activity logs
The logger SHALL write user activity events as individual JSON objects, each written as a single line terminated by a newline character (JSON Lines format).

#### Scenario: Session start event logged
- **WHEN** the activity logger starts for a user
- **THEN** it SHALL append a JSON line containing "type": "session_start", the current timestamp, and the username.

#### Scenario: Activity entry event logged
- **WHEN** the user switches focus to a different application (different process name) or logging stops
- **THEN** it SHALL append a JSON line containing the accumulated keystrokes, total mouse distance, start and end timestamps, the latest window title, and the process name for that application focus session.

### Requirement: Log file naming
The logger SHALL write activity logs to a file named `activity_log_<username>.jsonl` where `<username>` is the currently logged-in user.

#### Scenario: Verify file extension
- **WHEN** logging is started for username "jack"
- **THEN** the logger SHALL write events to a file named "activity_log_jack.jsonl".

### Requirement: Exception and error logging
The logger SHALL log exceptions and errors to a plain text file named `activity_error.txt` in a human-readable format.

#### Scenario: Verify error log format
- **WHEN** an exception occurs in the logger's window polling or timer routine
- **THEN** the logger SHALL append a plain text error line to "activity_error.txt" with a timestamp and error description.

### Requirement: Network connection polling
The system SHALL poll the Windows active connection tables periodically to capture TCP/UDP connections.

#### Scenario: Verify new connection logging
- **WHEN** a new TCP connection in established state or active UDP socket is found
- **THEN** the system SHALL resolve its owning PID to a process name and query the local domain-to-IP cache.

### Requirement: Domain resolution via DNS ETW cache
The system SHALL trace the `Microsoft-Windows-DNS-Client` ETW provider to dynamically build a local domain-to-IP resolution cache.

#### Scenario: Intercept DNS resolution
- **WHEN** a DNS client resolution event resolves a domain name (e.g. `google.com`) to an IP address
- **THEN** the system SHALL add the IP-to-Domain mapping to the lookup cache.

### Requirement: Network log file output
The system SHALL write network connection events to a separate JSON Lines file named `network_log_<username>.jsonl`.

#### Scenario: Verify log file separation
- **WHEN** logging network events for user "jack"
- **THEN** events SHALL be appended in compact JSON format with a trailing newline to "network_log_jack.jsonl".
