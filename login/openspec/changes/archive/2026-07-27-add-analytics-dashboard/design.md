## Context

The application is a Qt 5.15 / C++17 Windows desktop monitoring app. It currently outputs two main log streams per authenticated session in `logs/<username>/`:
1. `activity_log.jsonl`: Foreground window title, process name, start/end timestamps, raw keystroke buffer, mouse distance px.
2. `network_log.jsonl`: Session records (remote IP/port, attributed hostname & confidence, process PID/name/confidence/source, transport protocol, app protocol hint, bytes sent/received, packets, close reason).
3. `network_error.txt` & `network_dns_diag.jsonl`: Diagnostic and warning logs.

To provide end users with actionable insights without adding heavy runtime overhead or complex dependencies (such as QtWebEngine, local web servers, or cloud analytics), the app will generate a standalone `dashboard.html` file on demand or at session end, and launch it in the OS default browser.

## Goals / Non-Goals

**Goals:**
- Offline, self-contained single-file HTML/CSS/JS dashboard generation from local JSONL logs.
- Pure C++ pipeline (`AnalyticsLogReader` -> `AnalyticsAggregator` -> `HtmlDashboardGenerator`).
- Strict privacy: count keystrokes, do not display raw keystroke strings.
- Modular vertical slice architecture allowing gradual feature rollouts (Foundation -> Safe Activity -> Network -> App Usage -> Timeline -> Data Quality).
- Clear UI separation between Activity, Network, App Usage, Timeline, and Data Quality sections.
- Extensible reader interface so SQLite can replace or supplement JSONL readers in future iterations without changing aggregators or UI.

**Non-Goals:**
- No local web server or HTTP daemon (v1).
- No QtWebEngine widget embedded inside the Qt app (v1).
- No SQLite database migration (v1 - architectural hook only).
- No real-time streaming dashboard updates during active logging.
- No raw keylogging text display, URL browsing history, or payload inspection.
- No coupling of analytics logic to `ActivityLogger` or `NetworkOrchestrator`.

## Architecture & Data Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            LOG FILES ON DISK                                │
│   logs/<user>/activity_log.jsonl   network_log.jsonl   network_error.txt    │
└───────────────────────────────────┬─────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                          ANALYTICS READERS                                  │
│   ActivityLogReader      NetworkLogReader      NetworkErrorReader           │
└───────────────────────────────────┬─────────────────────────────────────────┘
                                    │ (Normalized Models)
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         ANALYTICS AGGREGATORS                               │
│   ActivityAggregator    NetworkAggregator    AppCorrelator    TimelineAgg. │
└───────────────────────────────────┬─────────────────────────────────────────┘
                                    │ (DashboardDataModel)
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                       HTML DASHBOARD GENERATOR                              │
│   HtmlDashboardGenerator + DashboardTemplate (Embedded CSS + Vanilla JS)    │
└───────────────────────────────────┬─────────────────────────────────────────┘
                                    │ (dashboard.html)
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        USER SYSTEM BROWSER                                  │
│   QDesktopServices::openUrl(file:///.../logs/<user>/dashboard.html)         │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Decisions

### Decision 1: Self-Contained Single-File HTML Output
**Choice**: The `HtmlDashboardGenerator` will produce a standalone `dashboard.html` with all CSS styling and vanilla JavaScript embedded inline (no external CDN links, no internet connectivity required).
**Rationale**: Ensures offline reliability, zero external network requests, instant load times, and simple distribution.
**Alternative Considered**: Using a lightweight embedded HTTP server (e.g. CivetWeb or QHttpServer). Rejected for v1 to avoid port binding issues, security risks, firewall prompts, and unnecessary process complexity.

### Decision 2: Abstract Reader Layer (`IAnalyticsReader`)
**Choice**: Define `IActivityLogReader` and `INetworkLogReader` pure virtual interfaces that read from JSONL files in v1.
**Rationale**: Decouples the aggregator and generator logic from the storage medium. When SQLite is introduced in a future release, new `SqliteActivityReader` implementations can be swapped in without modifying aggregators or generators.

### Decision 3: Safe Metric Aggregation (Privacy First)
**Choice**: `ActivityLogReader` reads the `keystrokes` field from `activity_log.jsonl`, calculates `keystrokes.length()`, and stores only `int keystrokeCount` in `ActivityRecord`. The raw string is immediately discarded.
**Rationale**: Protects user privacy while still giving users insight into productivity/activity levels.

### Decision 4: Heuristic App Usage Correlation
**Choice**: `AppCorrelator` maps network sessions to activity records by matching `process_name` (case-insensitive) and verifying time overlap (`session.startTime <= activity.endTime && session.endTime >= activity.startTime`).
**Rationale**: Activity logs and network logs originate from different system hooks. Explicitly labeling correlation as "Approximate" in the UI avoids misleading users while delivering meaningful combined metrics per application.

### Decision 5: Dedicated `DashboardController` in `ui/`
**Choice**: `MainWindow` delegates dashboard generation entirely to `DashboardController`.
```cpp
// In MainWindow slots:
void MainWindow::onViewDashboardClicked() {
    m_dashboardController->generateAndOpenDashboard(m_currentUser);
}
```
**Rationale**: Keeps `MainWindow` thin and avoids polluting MainWindow with file parsing, HTML generation, or aggregation code.

## Component Design

### Module Directory Structure

```
analytics/
├── model/
│   ├── analyticsmodels.h       // ActivityRecord, NetworkRecord, ErrorRecord
│   ├── activitysummary.h       // Aggregated activity metrics
│   ├── networksummary.h        // Aggregated network metrics
│   ├── appsummary.h            // Combined app metrics
│   └── timelinebucket.h        // Time-bucketed metrics
├── reader/
│   ├── activitylogreader.h/.cpp
│   ├── networklogreader.h/.cpp
│   └── networkerrorreader.h/.cpp
├── aggregator/
│   ├── activityaggregator.h/.cpp
│   ├── networkaggregator.h/.cpp
│   ├── appcorrelator.h/.cpp
│   └── timelineaggregator.h/.cpp
└── dashboard/
    ├── dashboarddatamodel.h/.cpp
    ├── htmldashboardgenerator.h/.cpp
    └── dashboardtemplate.h

ui/
├── dashboardcontroller.h/.cpp
```

## Risks / Trade-offs

- **Risk**: Large JSONL files (100MB+) could take multiple seconds to parse synchronously.
  - *Mitigation*: Run `AnalyticsLogReader` and aggregation in a background `QThread` or `QtConcurrent::run`, showing a progress/busy indicator in the UI.
- **Risk**: Clock drift or mismatched timestamps between Activity Logger (polling 2s) and WinDivert/ETW network sessions.
  - *Mitigation*: Use generous interval overlap checks (e.g. ±5s grace window) in `AppCorrelator` and display confidence badges ("High", "Approximate").
- **Risk**: Browser security blocking local file script execution.
  - *Mitigation*: Use plain vanilla JavaScript without ES modules (`<script type="module">` can fail on local `file://` URLs in some browsers).

## Migration Plan

- No existing file formats or logs are modified.
- Additive C++ components only.
- Future SQLite migration will simply provide `SqliteLogReader` implementing `IAnalyticsReader`.
