### Requirement: Structured ETW Network Telemetry Extraction
The system SHALL parse and extract structured network telemetry from ETW events into a unified data structure comprising: 5-tuple addressing, Process Attribution, Bandwidth & Throughput metrics, Transport Performance, and Application Layer Details.

#### Scenario: Extract transport layer and TCP performance telemetry
- **WHEN** ETW `Microsoft-Windows-TCPIP` events occur (e.g., Event IDs 1000-1030 for connect, disconnect, retransmit, send, receive)
- **THEN** the system SHALL extract Local IP, Local Port, Remote IP, Remote Port, Process ID (PID), Process Path, Bytes Sent/Received, TCP Round-Trip Time (RTT), RTT Variance, and Packet Retransmission count.

#### Scenario: Extract HTTP and Web application telemetry
- **WHEN** ETW `WinINet` or `Microsoft-Windows-WebIO` events occur
- **THEN** the system SHALL extract Target URL / Host, HTTP Method, HTTP Status Code, Content-Type, Request/Response Headers, Time-to-First-Byte (TTFB), and total transaction duration.

#### Scenario: Expose comprehensive ETW data taxonomy report
- **WHEN** the user requests the complete catalog of extractable ETW network data
- **THEN** the system SHALL output a detailed reference matrix mapping each provider (`TCPIP`, `WebIO`, `WinINet`, `NDIS`, `DNS-Client`), event ID, extracted parameters, and practical usage cases (e.g., protocol identification, latency monitoring, leak detection).

### Requirement: Cumulative Data Volume Measurement and Formatting
The system SHALL track the cumulative volume of data sent and received in bytes for each connection and display/log this volume formatted in human-readable KB/MB/GB units.

#### Scenario: Human-readable throughput unit formatting
- **WHEN** 1,500,000 bytes are received on a connection
- **THEN** the system SHALL display the metric as "1.43 MB" and record the raw value 1500000.

### Requirement: Consecutive Connection Consolidation
The system SHALL consolidate consecutive network connection events between identical Local IP and Remote IP endpoints into a single merged connection record.

#### Scenario: Consolidation of consecutive same-endpoint connections
- **WHEN** a process opens multiple sequential TCP connections from local IP `192.168.1.15` to remote IP `104.244.42.1` on ports `52001`, `52002`, and `52003`
- **THEN** the system SHALL output a single merged connection record containing the list of local ports `[52001, 52002, 52003]`, setting the aggregated byte counters to the cumulative sum of all three connections.
