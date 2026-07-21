## Why

Understanding active network connections and identifying their application-layer protocol (e.g., HTTP REST API, WebSocket, gRPC, DNS, TLS) alongside low-level socket metadata via Event Tracing for Windows (ETW) provides deep system visibility without requiring third-party driver installations (such as Npcap/WinPcap). Integrating ETW network tracing into the application enables real-time protocol identification, process-to-connection mapping, latency metrics, and extraction of HTTP/WebSocket network telemetry.

## What Changes

- Implement an Event Tracing for Windows (ETW) real-time session controller consuming events from `Microsoft-Windows-TCPIP`, `Microsoft-Windows-WebIO`, `WinINet`, and `Microsoft-Windows-NDIS-PacketCapture`.
- Implement a protocol classification engine that detects protocol types (REST API, WebSocket, HTTP/1.1, HTTP/2, gRPC, TLS/SSL, DNS, Raw TCP/UDP) based on ETW event IDs, payload inspection, TLS ALPN/SNI extensions, and HTTP header markers.
- Document and extract all rich network data fields obtainable through ETW (Local/Remote IP & Port, Process ID & Image Name, Bytes Sent/Received, TCP State Changes, RTT / Latency, Packet Retransmissions, HTTP Request/Response Headers, Status Codes, and WebSocket Frame Types).
- Calculate and format total data sent and received in human-readable KB/MB units per connection.
- Consolidate consecutive network log entries where both the local IP and remote IP match identically into a single merged connection record.
- Provide C++ / Qt integration interfaces to stream real-time connection telemetry to UI components and logger services.

## Capabilities

### New Capabilities
- `etw-network-tracing`: Real-time ETW trace session creation, provider subscription (`Microsoft-Windows-TCPIP`, `Microsoft-Windows-WebIO`, `WinINet`), and event processing loop.
- `protocol-classification`: Algorithmic protocol classification distinguishing REST API endpoints, WebSocket channels, HTTP streams, TLS handshakes, and generic TCP/UDP sockets.
- `network-data-extraction`: Structured data extraction schema covering connection lifetime, bandwidth usage, latency, process attribution, and application headers via ETW event properties.

### Modified Capabilities

## Impact

- Windows APIs: Uses Windows SDK headers (`evntrace.h`, `tdh.h`) and libraries (`tdh.lib`, `advapi32.lib`).
- Privileges: ETW real-time session control requires Administrator privileges or Performance Log Users group membership.
- Application codebase: Adds `etw/` and `network/` monitoring modules, exposed to existing Qt UI and logging subsystems.
