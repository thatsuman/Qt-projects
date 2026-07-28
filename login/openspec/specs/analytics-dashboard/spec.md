# analytics-dashboard Specification

## Purpose
TBD - created by archiving change add-analytics-dashboard. Update Purpose after archive.
## Requirements
### Requirement: Session analytics log reading

The system SHALL read and normalize existing JSONL activity logs, JSONL network logs, and text error logs for a user session without altering log files or active logging subsystems.

#### Scenario: Parse activity log
- **WHEN** `ActivityLogReader::readLogs` is invoked for a given username
- **THEN** it SHALL parse `logs/<username>/activity_log.jsonl` line by line
- **AND** extract `timestamp_start`, `timestamp_end`, `window_title`, `process_name`, `keystrokes`, and `mouse_distance_px`
- **AND** convert `keystrokes` to an integer length `keystrokeCount` while immediately discarding the raw string content

#### Scenario: Parse network log
- **WHEN** `NetworkLogReader::readLogs` is invoked for a given username
- **THEN** it SHALL parse `logs/<username>/network_log.jsonl` line by line
- **AND** extract remote endpoint, hostname, hostname confidence, process PID, process name, process confidence, process source, transport protocol, app protocol hint, bytes sent/received, packet counts, close reason, and optional DNS query info

#### Scenario: Parse network errors and diagnostics
- **WHEN** `NetworkErrorReader::readLogs` is invoked for a given username
- **THEN** it SHALL parse `logs/<username>/network_error.txt` and `logs/<username>/network_dns_diag.jsonl` if present
- **AND** count warning occurrences, DNS parse failures, and unparsed event shapes

---

### Requirement: Activity metric aggregation

The system SHALL aggregate activity records by application/process name to compute total active time, session count, keystroke count, and mouse distance.

#### Scenario: Per-app activity summary
- **WHEN** `ActivityAggregator::aggregate` is executed on a list of activity records
- **THEN** it SHALL group entries by `process_name`
- **AND** compute `totalActiveDurationSeconds`, `sessionCount`, `totalKeystrokes`, and `totalMouseDistancePixels` for each process
- **AND** compute overall total active time and session duration across all processes

---

### Requirement: Network metric aggregation

The system SHALL aggregate network session records by application/process, remote hostname/IP, protocol hint, and remote port.

#### Scenario: Per-app network summary
- **WHEN** `NetworkAggregator::aggregate` is executed on a list of network session records
- **THEN** it SHALL group entries by process name
- **AND** compute `bytesSentTotal`, `bytesReceivedTotal`, `totalBytes`, `packetsSent`, `packetsReceived`, and `sessionCount` per process

#### Scenario: Top remote endpoints summary
- **WHEN** `NetworkAggregator::aggregate` is executed
- **THEN** it SHALL group entries by remote hostname (or remote IP when hostname is absent)
- **AND** summarize total traffic volume, hostname confidence tier, and protocol hints used for each remote destination

---

### Requirement: App usage correlation

The system SHALL combine activity metrics and network metrics per application process using process name and time overlap matching.

#### Scenario: Correlate activity and network per app
- **WHEN** `AppCorrelator::correlate` processes aggregated activity and network summaries
- **THEN** it SHALL match entries where process names match (case-insensitive) and activity window overlaps with network session window
- **AND** produce a unified `AppSummary` containing active duration, keystroke count, mouse distance, bytes sent, bytes received, and session counts
- **AND** tag the correlation with confidence level `"Approximate"`

---

### Requirement: Timeline bucket aggregation

The system SHALL aggregate activity and network traffic into uniform time buckets (default 1-minute intervals) for timeline visualization.

#### Scenario: 1-minute bucket aggregation
- **WHEN** `TimelineAggregator::aggregate` is executed with a 60-second interval
- **THEN** it SHALL divide the total session time span into 60-second buckets
- **AND** assign foreground application activity, bytes sent, and bytes received to each bucket based on record timestamps

---

### Requirement: Self-contained HTML dashboard generation

The system SHALL export aggregated analytics data into a single, offline, self-contained HTML dashboard file and launch it in the default system browser.

#### Scenario: Generate dashboard HTML file
- **WHEN** `HtmlDashboardGenerator::generate` is called with a `DashboardDataModel`
- **THEN** it SHALL generate `logs/<username>/dashboard.html` containing embedded CSS, JSON data payload, and vanilla JavaScript
- **AND** it SHALL NOT require external web servers, CDN links, internet access, or QtWebEngine

#### Scenario: Dashboard sections render
- **WHEN** the generated `dashboard.html` is opened in a web browser
- **THEN** it SHALL display 6 distinct tabbed sections: Overview, Activity, Network, App Usage, Timeline, and Data Quality
- **AND** interactive tables SHALL support client-side searching, sorting, and filtering

#### Scenario: UI launch trigger
- **WHEN** the user clicks "View Dashboard" in MainWindow
- **THEN** `DashboardController` SHALL execute log reading, aggregation, HTML generation, and invoke `QDesktopServices::openUrl` pointing to `dashboard.html`

