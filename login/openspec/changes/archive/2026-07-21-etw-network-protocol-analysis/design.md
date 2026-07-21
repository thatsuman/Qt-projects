## Context

Event Tracing for Windows (ETW) is a high-performance, low-overhead tracing mechanism built directly into the Windows kernel and system components. Unlike packet capturing tools (e.g., Wireshark/Npcap) which require custom kernel drivers and raw frame parsing, ETW allows applications to consume structured events emitted directly by Windows TCPIP drivers, WinINet, WebIO, NDIS, and DNS Client components.

This design document outlines how our application consumes ETW events to:
1. Determine the exact application protocol (REST API vs. WebSocket vs. HTTP/1.1 vs. HTTP/2 vs. TLS vs. TCP/UDP) used by every active network connection.
2. Comprehensive taxonomy of all network telemetry extractable via ETW.

## Goals / Non-Goals

**Goals:**
- Provide a C++ / Qt ETW real-time session controller (`EtwTraceSession`) that listens to ETW providers without installing kernel drivers.
- Implement a protocol classification engine (`ProtocolClassifier`) distinguishing REST API calls from WebSockets and raw socket connections.
- Document and extract full ETW network telemetry (addresses, ports, PID, process path, throughput, latency/RTT, TLS SNI, HTTP URLs/status codes/headers).
- Integrate ETW telemetry output into Qt logger and application window.

**Non-Goals:**
- Decrypting arbitrary TLS payload contents (ETW extracts HTTP/WebSocket metadata from WinINet/WebIO or TLS SNI/ALPN, avoiding raw payload MITM decryption).
- Modifying or blocking network packets (ETW is strictly passive monitoring).

## Decisions

### Decision 1: Multi-Provider ETW Architecture
We subscribe to multiple complementary ETW providers to capture both low-level transport metrics and high-level application events:

| Provider Name | Provider GUID | Purpose & Extracted Data |
| :--- | :--- | :--- |
| `Microsoft-Windows-TCPIP` | `{2F07E2EE-15DB-4B1F-B6A0-B6328C2A578C}` | Socket creation, connect/disconnect, IP/Ports, PID, Bytes Sent/Recv, RTT, Retransmissions. |
| `Microsoft-Windows-WebIO` | `{5088210A-963F-457F-B04F-D86736517786}` | HTTP/1.1 & HTTP/2 requests, WebSocket handshake, WebSocket frame headers, HTTP response status. |
| `WinINet` | `{43D2A454-5A0F-4696-92A7-11397EC3D2A7}` | WinHTTP/WinINet API calls, URL requests, HTTP headers, cookies, cache stats, REST API transactions. |
| `Microsoft-Windows-DNS-Client` | `{1C95126E-7EEA-49A9-A3FE-A378B03DDB4D}` | Hostname to IP resolution queries and responses. |
| `Microsoft-Windows-NDIS-PacketCapture` | `{2604E73D-6481-4B36-A287-C0DB2765A256}` | Lightweight packet headers (Ethernet/IP/TCP) when full payload inspection is enabled. |

*Alternative Considered*: Using WinPcap/Npcap driver capture. Rejected because it requires third-party driver installation, admin prompt during install, and lacks process ID (PID) correlation.

### Decision 2: Protocol Identification Matrix (REST API vs. WebSocket vs. Others)

To determine which protocol is being used for each network connection, we use a 4-tiered classification algorithm:

```
                  [ Active Connection (5-Tuple: Local IP/Port, Remote IP/Port, PID) ]
                                                │
                                  ETW Event Stream Received
                                                │
                 ┌──────────────────────────────┴──────────────────────────────┐
                 ▼                                                             ▼
     [ WinINet / WebIO Event? ]                                  [ TCPIP Kernel Event Only? ]
                 │                                                             │
        ┌────────┴────────┐                                           ┌────────┴────────┐
        ▼                 ▼                                           ▼                 ▼
[ WebSocket Event ]  [ HTTP Request ]                               [ Port / TLS SNI ] [ Payload Signature ]
(Upgrade: websocket   (REST Method: GET/POST,                         - Port 443 + SNI  - TLS Client Hello
 or WebIO Frame)       JSON/XML Content-Type)                         - Port 80, 53     - Plaintext Magic Bytes
        │                 │                                           │                 │
        ▼                 ▼                                           ▼                 ▼
   WebSocket          REST API                                      TLS / DNS         Generic TCP/UDP
```

1. **REST API Detection**:
   - **Trigger**: `WinINet` / `WebIO` events or payload HTTP headers.
   - **Criteria**: Request contains HTTP methods (`GET`, `POST`, `PUT`, `DELETE`, `PATCH`), structured paths (e.g. `/api/v1/...`), and Content-Type `application/json`, `application/xml`, or `application/x-www-form-urlencoded`.
2. **WebSocket Detection**:
   - **Trigger**: `WebIO` WebSocket events or HTTP headers.
   - **Criteria**: Connection HTTP handshake contains `Upgrade: websocket` and `Connection: Upgrade` headers. Subsequent ETW WebIO events record opcode frames (Text, Binary, Ping, Pong, Close).
3. **HTTP/1.1 vs HTTP/2 vs gRPC**:
   - **HTTP/2 & gRPC**: WebIO/WinINet events recording multiplexed stream IDs or Content-Type `application/grpc`.
4. **TLS / SSL Identification**:
   - **Criteria**: TCPIP port 443 + TLS handshake Client Hello extracting SNI hostname and ALPN extension (`h2`, `http/1.1`).

### Decision 3: Comprehensive Taxonomy of ETW Network Data

The following table details **all network data fields** extractable via ETW:

| Category | Extracted Data Field | Source ETW Provider | Description & Usage |
| :--- | :--- | :--- | :--- |
| **Addressing & Identity** | Local IP Address | `TCPIP` | IPv4 or IPv6 local socket endpoint |
| | Local Port | `TCPIP` | Ephemeral or listening port |
| | Remote IP Address | `TCPIP` | Destination server IP |
| | Remote Port | `TCPIP` | Target port (80, 443, 8080, etc.) |
| | Process ID (PID) | `TCPIP`, `WebIO` | Process generating traffic |
| | Process Image Path | `TCPIP` / System | Executable name (e.g., `chrome.exe`, `login.exe`) |
| **Transport & Metrics** | Bytes Sent / Received | `TCPIP` | Throughput and volume per connection |
| | Bytes Sent/Received (Formatted) | Calculation | Human-readable string representation (e.g. "1.24 MB", "42.5 KB") |
| | Connection Count | Aggregating | Total number of consolidated connection events |
| | TCP State | `TCPIP` | Closed, Listen, SynSent, SynReceived, Established, FinWait |
| | Round-Trip Time (RTT) | `TCPIP` | Smoothed RTT in milliseconds |
| | RTT Variance | `TCPIP` | Jitter / latency stability |
| | Packet Retransmissions | `TCPIP` | Loss rate and network congestion indicator |
| | TCP Window Size | `TCPIP` | Flow control window size |
| **Application & Web** | Target URL / Host | `WinINet`, `WebIO` | Full requested URL or domain name |
| | HTTP Method | `WinINet`, `WebIO` | `GET`, `POST`, `PUT`, `DELETE`, `OPTIONS` |
| | HTTP Response Code | `WinINet`, `WebIO` | Status code (200 OK, 401 Unauthorized, 500 Error) |
| | Request/Response Headers | `WinINet`, `WebIO` | `Content-Type`, `User-Agent`, `Authorization`, `Upgrade` |
| | Latency (TTFB) | `WinINet`, `WebIO` | Time-to-First-Byte from request to initial header |
| | WebSocket Opcode | `WebIO` | Frame types: Text (0x1), Binary (0x2), Ping (0x9), Pong (0xA) |
| **Security & Domain** | DNS Hostname Query | `DNS-Client` | Domain queried before connection establishment |
| | TLS SNI Hostname | `WebIO` / `TCPIP` | Target domain in encrypted TLS Client Hello |
| | TLS ALPN Protocol | `WebIO` | Negotiated protocol (`h2`, `http/1.1`, `spdy`) |

### Decision 4: In-Memory Connection Consolidation Flow

To prevent log pollution and excessive diagnostic chattiness during rapid, consecutive TCP socket connections (e.g. browsers spawning multiple parallel connections to download webpage assets), we group consecutive TCP events to identical IP endpoints.

```
Incoming Event (Local IP:Local Port -> Remote IP:Remote Port)
                           │
       Check last connection in session list for PID
                           │
      Do both Local IP and Remote IP match identically?
                           │
               ┌───────────┴───────────┐
               ▼ YES                   ▼ NO
     Merge event:            Create new record:
     - Append to localPorts  - Initialize bytes
     - Sum bytesSent/Recv    - Set localIp, remoteIp
     - Update lastSeen       - Set firstSeen = now
```

This consolidation runs inside the `PidSession` manager on the main logging loop, serializing lists of local/remote ports rather than distinct 5-tuple log records.

## Risks / Trade-offs

- **[Risk: Privilege Requirement]** → ETW real-time tracing requires Administrator rights or Performance Log Users group.
  - *Mitigation*: Application will check token privileges at startup and display clear instructions if elevation is required.
- **[Risk: Event Volume at High Network Speed]** → Real-time `ProcessTrace` can drop events under gigabit throughput.
  - *Mitigation*: Configure ETW buffer size (`EVENT_TRACE_PROPERTIES.BufferSize = 64KB`, `MinimumBuffers = 16`, `MaximumBuffers = 128`) and process events asynchronously on a background worker thread.

## Migration Plan

1. Add ETW provider declarations (`etw_providers.h`) and `WinSdk` linkage (`tdh.lib`, `advapi32.lib`).
2. Implement `EtwTraceSession` worker thread.
3. Implement `ProtocolClassifier` with REST vs WebSocket parser.
4. Expose UI tab / telemetry logger displaying live connections, classified protocols, and extracted data.
