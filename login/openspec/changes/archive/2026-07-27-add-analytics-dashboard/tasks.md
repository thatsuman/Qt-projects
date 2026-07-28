## 1. Analytics Data Models and Interfaces

- [x] 1.1 Create `analytics/model/analyticsmodels.h` defining normalized `ActivityRecord`, `NetworkRecord`, and `ErrorRecord` data structures
- [x] 1.2 Create `analytics/model/activitysummary.h`, `networksummary.h`, `appsummary.h`, and `timelinebucket.h` structs
- [x] 1.3 Create abstract interfaces `IActivityLogReader` and `INetworkLogReader` to decouple storage strategy (JSONL vs SQLite)

## 2. JSONL Log Readers

- [x] 2.1 Implement `ActivityLogReader` in `analytics/reader/activitylogreader.h/.cpp` to parse `activity_log.jsonl` (derive keystroke count, discard raw keystroke text)
- [x] 2.2 Implement `NetworkLogReader` in `analytics/reader/networklogreader.h/.cpp` to parse `network_log.jsonl`
- [x] 2.3 Implement `NetworkErrorReader` in `analytics/reader/networkerrorreader.h/.cpp` to parse `network_error.txt` and `network_dns_diag.jsonl`

## 3. Analytics Aggregators

- [x] 3.1 Implement `ActivityAggregator` in `analytics/aggregator/activityaggregator.h/.cpp` to group activity records by process name
- [x] 3.2 Implement `NetworkAggregator` in `analytics/aggregator/networkaggregator.h/.cpp` to group network records by process and remote hostname/IP
- [x] 3.3 Implement `AppCorrelator` in `analytics/aggregator/appcorrelator.h/.cpp` to combine activity and network metrics per application
- [x] 3.4 Implement `TimelineAggregator` in `analytics/aggregator/timelineaggregator.h/.cpp` to bucket metrics into 1-minute intervals

## 4. Self-Contained HTML Dashboard Generator

- [x] 4.1 Create `DashboardDataModel` in `analytics/dashboard/dashboarddatamodel.h/.cpp` summarizing all section payloads into a unified JSON structure
- [x] 4.2 Create `DashboardTemplate` in `analytics/dashboard/dashboardtemplate.h` holding embedded CSS styles and vanilla JS interactive scripts
- [x] 4.3 Implement `HtmlDashboardGenerator` in `analytics/dashboard/htmldashboardgenerator.h/.cpp` to produce `logs/<username>/dashboard.html`

## 5. UI Controller & MainWindow Integration

- [x] 5.1 Create `DashboardController` in `ui/dashboardcontroller.h/.cpp` to orchestrate background reading, aggregation, HTML generation, and browser launch
- [x] 5.2 Add "View Dashboard" button/menu action in `mainwindow.ui` and wire slot in `mainwindow.cpp` to call `DashboardController`
- [x] 5.3 Add new source and header files to `login.pro` qmake build configuration

## 6. Dashboard Sections & Interactive Verification

- [x] 6.1 Verify Overview section displays session duration, total active time, network upload/download totals, and top apps
- [x] 6.2 Verify Activity section shows app names, active duration, session count, keystroke counts (no raw keystrokes), and mouse distance
- [x] 6.3 Verify Network section displays process names, remote hostnames, IP fallbacks, ports, protocol hints, bytes sent/received, and close reasons
- [x] 6.4 Verify App Usage section displays combined activity and network metrics with approximate correlation labels
- [x] 6.5 Verify Timeline section displays 1-minute interval activity and network usage blocks
- [x] 6.6 Verify Data Quality section displays unknown hostname/process percentages, DNS parser warning counts, and diagnostic indicators
- [x] 6.7 Verify interactive table searching, column sorting, and section tab switching work cleanly in standard web browsers
