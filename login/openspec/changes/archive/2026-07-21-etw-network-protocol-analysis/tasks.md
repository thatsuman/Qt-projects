## 1. ETW Infrastructure & Real-Time Session Controller

- [x] 1.1 Create `etw/etw_providers.h` defining GUIDs for `TCPIP`, `WebIO`, `WinINet`, `DNS-Client`, and `NDIS` ETW providers.
- [x] 1.2 Implement `etw/EtwTraceSession.h` and `etw/EtwTraceSession.cpp` using Windows `StartTraceW`, `EnableTraceEx2`, `OpenTraceW`, and `ProcessTrace`.
- [x] 1.3 Implement background worker thread execution for `ProcessTrace` with asynchronous event queueing.
- [x] 1.4 Add administrative token privilege validation and error reporting when launching ETW trace sessions without elevation.

## 2. Protocol Classification Engine

- [x] 2.1 Implement `network/ProtocolClassifier.h` and `network/ProtocolClassifier.cpp` with protocol enum types (`REST_API`, `WEBSOCKET`, `HTTP1_1`, `HTTP2`, `GRPC`, `TLS`, `DNS`, `RAW_TCP`, `RAW_UDP`).
- [x] 2.2 Implement REST API heuristics parsing HTTP methods (`GET`, `POST`, `PUT`, `DELETE`), RESTful URL patterns, and JSON/XML content headers.
- [x] 2.3 Implement WebSocket handshake parser checking `Upgrade: websocket` headers and WebIO frame opcode events (Text, Binary, Ping/Pong).
- [x] 2.4 Implement TLS Client Hello / Server Hello inspector to extract SNI hostnames and ALPN extension strings (`h2`, `http/1.1`).
- [x] 2.5 Implement dynamic protocol transition state tracking for active 5-tuples (Local IP/Port, Remote IP/Port, PID).

## 3. Network Telemetry Data Extraction

- [x] 3.1 Implement structured data extraction models (`NetworkConnectionRecord`, `HttpTelemetryRecord`, `TcpMetricsRecord`).
- [x] 3.2 Extract transport-layer telemetry from TCPIP ETW events (IPs, Ports, PID, Process Name, Bytes Sent/Received, RTT, RTT Variance, Retransmission count).
- [x] 3.3 Implement helper functions to format transfer bytes into human-readable strings (KB/MB/GB).
- [x] 3.4 Implement in-memory connection consolidation logic checking for identical consecutive local and remote IP addresses.
- [x] 3.5 Extract application-layer telemetry from WinINet/WebIO ETW events (Request URL, HTTP Method, Status Code, Content-Type, Headers, TTFB Latency).
- [x] 3.6 Build comprehensive ETW network data reference taxonomy catalog.

## 4. Qt Subsystem Integration & UI Telemetry View

- [x] 4.1 Update project file `login.pro` to link `advapi32.lib`, `tdh.lib`, and `ws2_32.lib`.
- [x] 4.2 Integrate `EtwTraceSession` signal-slot events with `MainWindow` Qt interface to display live active connections, protocol badges, and bandwidth meters.
- [x] 4.3 Add network telemetry logging to existing `logger` module.

## 5. Verification & Testing

- [x] 5.1 Verify REST API vs WebSocket protocol classification against live test endpoints.
- [x] 5.2 Validate process attribution (PID to process executable path mapping).
- [x] 5.3 Verify that consecutive connections to identical endpoints are successfully consolidated and bandwidth is correctly aggregated in logging output.
- [x] 5.4 Validate performance under sustained network throughput.
