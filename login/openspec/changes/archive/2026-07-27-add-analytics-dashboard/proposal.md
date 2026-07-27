## Why

The desktop monitoring application logs rich user activity (`activity_log.jsonl`) and network session events (`network_log.jsonl`). However, currently there is no end-user interface to view, analyze, or summarize this information. Raw JSONL files are difficult for users to interpret. An offline, interactive, self-contained HTML dashboard will provide clear after-session visibility into active duration, top applications, network usage, combined app activity, timeline trends, and data quality indicators without introducing complex dependencies like web servers or QtWebEngine.

## What Changes

- **Self-contained HTML Dashboard Generation**: Introduce a C++ analytics subsystem (`analytics/`) that reads JSONL logs and errors for a user session and generates a single, offline `dashboard.html` file in `logs/<username>/`.
- **Decoupled Architecture**: All reading, normalizing, aggregating, and HTML generating is isolated in `analytics/` and UI orchestration in `ui/dashboardcontroller.*`. Active loggers (`ActivityLogger`, `NetworkOrchestrator`) and capture layers are completely untouched.
- **Privacy-Safe Activity Metrics**: Only derived metrics (active window time, process session counts, total keystroke count, mouse distance in pixels) are extracted. Raw keystroke strings are discarded during log ingestion.
- **Network & Activity Correlation**: Combines process names and temporal overlaps between user activity and network sessions to produce approximate per-app usage metrics.
- **Interactive Offline UI**: The generated HTML includes embedded CSS and vanilla JavaScript for tabs, sorting, filtering, and data quality indicators, opening in the system's default browser via `QDesktopServices::openUrl`.
- **Data Quality Panel**: Parses `network_error.txt` and diagnostic logs to summarize unknown hostname/process rates, DNS warnings, and shutdown-flushed session counts.

## Capabilities

### New Capabilities

- `analytics-dashboard`: Self-contained, web-based local analytics dashboard generated from JSONL logs, covering Overview, Activity, Network, App Usage, Timeline, and Data Quality.

### Modified Capabilities

- `network-logging`: No requirement changes (the dashboard consumes output records written by network logging without altering capture or writer behavior).

## Impact

- **`analytics/`**: New module directory containing model, reader, aggregator, and dashboard generation headers/sources.
- **`ui/dashboardcontroller.*`**: New UI controller class orchestrating dashboard generation and browser launch.
- **`mainwindow.*` & `mainwindow.ui`**: Add a "View Dashboard" button/action in MainWindow that triggers `DashboardController`.
- **`login.pro`**: Add new C++ headers and source files to qmake build manifest.
- **Zero impact on privacy**: No raw keystrokes exported, no payload inspection, no browser history, no remote network calls.
