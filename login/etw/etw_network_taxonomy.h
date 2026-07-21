/*
 * etw_network_taxonomy.h
 *
 * Comprehensive ETW Network Data Reference Taxonomy Catalog
 * ==========================================================
 * This catalog documents all network telemetry fields extractable via
 * Event Tracing for Windows (ETW) from Windows built-in network providers.
 *
 * Providers covered:
 *   - Microsoft-Windows-TCPIP        (transport-layer events)
 *   - Microsoft-Windows-WebIO        (HTTP/2, WebSocket application-layer events)
 *   - Microsoft-Windows-WinINet      (WinHTTP/WinINet API-level HTTP events)
 *   - Microsoft-Windows-DNS-Client   (DNS resolution events)
 *   - Microsoft-Windows-NDIS-PacketCapture (lightweight packet header capture)
 *
 * Usage:
 *   Include this header for documentation and constant definitions.
 *   Field names (ETW property names) can be passed to TdhGetProperty().
 */

#ifndef ETW_NETWORK_TAXONOMY_H
#define ETW_NETWORK_TAXONOMY_H

// ============================================================================
// SECTION 1 — Microsoft-Windows-TCPIP Provider
// GUID: {2F07E2EE-15DB-4B1F-B6A0-B6328C2A578C}
// Purpose: Low-level kernel TCP/IP events: socket lifecycle, data transfers,
//          TCP state changes, RTT, retransmissions, window size.
// Privilege Required: Administrator or Performance Log Users
// ============================================================================

// ── TCPIP Event IDs ─────────────────────────────────────────────────────────
// Reference: https://docs.microsoft.com/windows/win32/etw/tcpip
#define ETW_TCPIP_EVENTID_SEND                  10   // IPv4 TCP send
#define ETW_TCPIP_EVENTID_RECEIVE               11   // IPv4 TCP receive
#define ETW_TCPIP_EVENTID_CONNECT               12   // IPv4 TCP connect
#define ETW_TCPIP_EVENTID_DISCONNECT            13   // IPv4 TCP disconnect
#define ETW_TCPIP_EVENTID_RETRANSMIT            14   // IPv4 TCP retransmit
#define ETW_TCPIP_EVENTID_SEND_IPV6             26   // IPv6 TCP send
#define ETW_TCPIP_EVENTID_RECEIVE_IPV6          27   // IPv6 TCP receive
#define ETW_TCPIP_EVENTID_CONNECT_IPV6          28   // IPv6 TCP connect
#define ETW_TCPIP_EVENTID_DISCONNECT_IPV6       29   // IPv6 TCP disconnect
#define ETW_TCPIP_EVENTID_RETRANSMIT_IPV6       30   // IPv6 TCP retransmit
#define ETW_TCPIP_EVENTID_UDP_SEND              42   // IPv4 UDP send
#define ETW_TCPIP_EVENTID_UDP_RECEIVE           43   // IPv4 UDP receive
#define ETW_TCPIP_EVENTID_UDP_SEND_IPV6         58   // IPv6 UDP send
#define ETW_TCPIP_EVENTID_UDP_RECEIVE_IPV6      59   // IPv6 UDP receive
#define ETW_TCPIP_EVENTID_TCP_FULL_SEND         1001 // Full TCP send (WFP/TDH-tagged)
#define ETW_TCPIP_EVENTID_TCP_FULL_RECV         1002 // Full TCP receive (WFP/TDH-tagged)

// ── TCPIP Property Names (pass to TdhGetProperty) ───────────────────────────
// Addressing & Identity
#define PROP_TCPIP_SADDR        L"saddr"         // Source (local) IPv4 address (4 bytes raw)
#define PROP_TCPIP_DADDR        L"daddr"         // Destination (remote) IPv4 address (4 bytes raw)
#define PROP_TCPIP_SPORT        L"sport"         // Source (local) port (uint16, network byte order)
#define PROP_TCPIP_DPORT        L"dport"         // Destination (remote) port (uint16, network byte order)
#define PROP_TCPIP_PID          L"PID"           // Process ID generating the event (DWORD)
#define PROP_TCPIP_PROC_NAME    L"ImageName"     // Process executable name (wchar_t string)

// Transport & Metrics
#define PROP_TCPIP_SIZE         L"size"          // Bytes in this segment (uint32)
#define PROP_TCPIP_SEQNO        L"seqno"         // TCP sequence number (uint32)
#define PROP_TCPIP_CONNID       L"connid"        // Connection identifier (uint64, unique per socket lifetime)
#define PROP_TCPIP_SRTT         L"srtt"          // Smoothed Round-Trip Time in milliseconds (uint32)
#define PROP_TCPIP_RTT_VAR      L"RttVar"        // RTT variance / jitter (uint32)
#define PROP_TCPIP_REXMIT       L"RexmitCount"   // Packet retransmission counter (uint32)
#define PROP_TCPIP_WSIZE        L"SndWnd"        // TCP send window size in bytes (uint32)
#define PROP_TCPIP_STATE        L"tcpState"      // TCP connection state enum (uint8):
                                                 //   0=Closed, 1=Listen, 2=SynSent, 3=SynRcvd,
                                                 //   4=Established, 5=FinWait1, 6=FinWait2,
                                                 //   7=CloseWait, 8=Closing, 9=LastAck, 10=TimeWait

// IPv6 addressing (16 bytes raw)
#define PROP_TCPIP_SADDR6       L"saddr6"        // Source IPv6 address (16 bytes raw)
#define PROP_TCPIP_DADDR6       L"daddr6"        // Destination IPv6 address (16 bytes raw)


// ============================================================================
// SECTION 2 — Microsoft-Windows-WebIO Provider
// GUID: {5088210A-963F-457F-B04F-D86736517786}
// Purpose: Application-layer HTTP/1.1, HTTP/2, and WebSocket events emitted
//          by the Windows WebIO component used by WinINet-based applications.
// Privilege Required: Standard user (user-mode provider)
// ============================================================================

// ── WebIO Event IDs ──────────────────────────────────────────────────────────
#define ETW_WEBIO_EVENTID_HTTP_REQUEST          1    // HTTP request initiated
#define ETW_WEBIO_EVENTID_HTTP_RESPONSE         2    // HTTP response header received
#define ETW_WEBIO_EVENTID_WS_HANDSHAKE_SEND     10   // WebSocket handshake sent (Upgrade request)
#define ETW_WEBIO_EVENTID_WS_HANDSHAKE_RECV     11   // WebSocket handshake response received (101 Switching)
#define ETW_WEBIO_EVENTID_WS_FRAME_SEND         20   // WebSocket data frame sent
#define ETW_WEBIO_EVENTID_WS_FRAME_RECV         21   // WebSocket data frame received
#define ETW_WEBIO_EVENTID_CONNECTION_OPEN       30   // HTTP connection opened
#define ETW_WEBIO_EVENTID_CONNECTION_CLOSE      31   // HTTP connection closed

// ── WebIO Property Names ─────────────────────────────────────────────────────
// Application & Web fields
#define PROP_WEBIO_URL          L"Url"           // Full requested URL (wchar_t string)
#define PROP_WEBIO_HOST         L"HostName"      // HTTP Host header value (wchar_t string)
#define PROP_WEBIO_METHOD       L"Method"        // HTTP verb: GET, POST, PUT, DELETE, PATCH (wchar_t)
#define PROP_WEBIO_STATUS_CODE  L"StatusCode"    // HTTP response status code (uint32, e.g. 200, 401, 500)
#define PROP_WEBIO_CONTENT_TYPE L"ContentType"   // Response Content-Type header (wchar_t string)
#define PROP_WEBIO_HEADERS      L"Headers"       // Raw request/response headers block (wchar_t string)
#define PROP_WEBIO_TTFB         L"TTFB"          // Time-to-First-Byte latency in milliseconds (uint64)
#define PROP_WEBIO_PROTO        L"Protocol"      // Negotiated protocol string: "h2", "http/1.1" (wchar_t)
#define PROP_WEBIO_SNI          L"ServerName"    // TLS SNI hostname (wchar_t string)
#define PROP_WEBIO_ALPN         L"AlpnResult"    // ALPN negotiation result: "h2", "http/1.1" (wchar_t)

// WebSocket-specific fields
#define PROP_WEBIO_WS_OPCODE    L"Opcode"        // WebSocket frame opcode (uint8):
                                                 //   0x0=Continuation, 0x1=Text, 0x2=Binary,
                                                 //   0x8=Close, 0x9=Ping, 0xA=Pong
#define PROP_WEBIO_WS_FIN       L"FinalFragment" // WebSocket FIN bit set (uint8: 0 or 1)
#define PROP_WEBIO_WS_PAYLEN    L"PayloadLength" // WebSocket frame payload length in bytes (uint64)
#define PROP_WEBIO_UPGRADE_HDR  L"UpgradeHeader" // Value of Upgrade header ("websocket") (wchar_t)

// Process identity (inherited from event header)
// Note: EventHeader.ProcessId provides the PID for WebIO events.


// ============================================================================
// SECTION 3 — Microsoft-Windows-WinINet Provider
// GUID: {43D2A454-5A0F-4696-92A7-11397EC3D2A7}
// Purpose: WinHTTP / WinINet API-level network request/response events,
//          including cookies, cache statistics, and HTTP transactions.
// Privilege Required: Standard user (user-mode provider)
// ============================================================================

// ── WinINet Event IDs ────────────────────────────────────────────────────────
#define ETW_WININET_EVENTID_REQUEST_START       1    // HTTP request started (URL, method)
#define ETW_WININET_EVENTID_REQUEST_COMPLETE    2    // HTTP transaction complete (status, bytes)
#define ETW_WININET_EVENTID_RESPONSE_HEADER     3    // Response header received
#define ETW_WININET_EVENTID_CACHE_HIT           10   // Cache hit: response served from local cache
#define ETW_WININET_EVENTID_CACHE_MISS          11   // Cache miss: full network round-trip required
#define ETW_WININET_EVENTID_COOKIE_SEND         20   // Cookie transmitted in request headers
#define ETW_WININET_EVENTID_COOKIE_RECV         21   // Cookie received from server response

// ── WinINet Property Names ───────────────────────────────────────────────────
#define PROP_WININET_URL          L"URL"             // Full request URL including query string (wchar_t)
#define PROP_WININET_VERB         L"Verb"            // HTTP method: GET, POST, PUT, DELETE (wchar_t)
#define PROP_WININET_STATUS       L"ResponseCode"    // HTTP status code (uint32)
#define PROP_WININET_CONTENT_TYPE L"ContentType"     // Response Content-Type (wchar_t)
#define PROP_WININET_BYTES_OUT    L"BytesSent"       // Total request bytes sent (uint64)
#define PROP_WININET_BYTES_IN     L"BytesReceived"   // Total response bytes received (uint64)
#define PROP_WININET_LATENCY      L"Latency"         // Round-trip latency in milliseconds (uint32)
#define PROP_WININET_PROXY        L"ProxyServer"     // Proxy server hostname if routing through proxy (wchar_t)
#define PROP_WININET_CACHE_ENTRY  L"CacheEntry"      // Cache key / URL (wchar_t)
#define PROP_WININET_COOKIE_NAME  L"CookieName"      // Cookie name (wchar_t)
#define PROP_WININET_COOKIE_VALUE L"CookieValue"     // Cookie value (wchar_t)
#define PROP_WININET_AUTH_SCHEME  L"AuthScheme"      // HTTP auth scheme: Basic, NTLM, Negotiate (wchar_t)


// ============================================================================
// SECTION 4 — Microsoft-Windows-DNS-Client Provider
// GUID: {1C95B24C-795E-4C84-B4A1-F23A2E7DB224}
// Purpose: DNS resolution queries and responses: hostname→IP lookups,
//          TTL, query types, and resolver latency.
// Privilege Required: Standard user (user-mode provider)
// ============================================================================

// ── DNS-Client Event IDs ─────────────────────────────────────────────────────
#define ETW_DNS_EVENTID_QUERY_START    1   // DNS query initiated
#define ETW_DNS_EVENTID_QUERY_SUCCESS  2   // DNS query returned results
#define ETW_DNS_EVENTID_QUERY_FAILED   3   // DNS query failed (NXDOMAIN or timeout)
#define ETW_DNS_EVENTID_CACHE_HIT      4   // DNS answer served from client cache

// ── DNS-Client Property Names ────────────────────────────────────────────────
#define PROP_DNS_QUERY_NAME   L"QueryName"     // Hostname being resolved (wchar_t)
#define PROP_DNS_QUERY_TYPE   L"QueryType"     // RR type: A (1), AAAA (28), CNAME (5), MX (15), TXT (16) (uint16)
#define PROP_DNS_QUERY_OPTS   L"QueryOptions"  // Resolver option flags (uint32)
#define PROP_DNS_RESULT_NAME  L"QueryResults"  // Resolved addresses / CNAME chain (wchar_t, semicolon-separated)
#define PROP_DNS_STATUS       L"Status"        // Win32 error code: 0=success, 9003=NXDOMAIN (uint32)
#define PROP_DNS_TTL          L"RecordTTL"     // DNS record Time-To-Live in seconds (uint32)
#define PROP_DNS_LATENCY_MS   L"QueryLatency"  // Time from query initiation to result, ms (uint32)


// ============================================================================
// SECTION 5 — Microsoft-Windows-NDIS-PacketCapture Provider
// GUID: {2604E73D-6481-4B36-A287-C0DB2765A256}
// Purpose: Lightweight Ethernet/IP/TCP packet header capture without a
//          third-party kernel driver (read-only, passive monitoring only).
// Privilege Required: Administrator
// Note: Full payload capture requires enabling "CaptureData" keyword.
//       Default captures headers only (first 64–128 bytes).
// ============================================================================

// ── NDIS Event IDs ───────────────────────────────────────────────────────────
#define ETW_NDIS_EVENTID_PACKET_SEND    1001  // Network interface outbound packet
#define ETW_NDIS_EVENTID_PACKET_RECV    1002  // Network interface inbound packet

// ── NDIS Property Names ──────────────────────────────────────────────────────
#define PROP_NDIS_FRAGMENT      L"Fragment"    // Raw packet header bytes (byte array, up to 128 bytes)
#define PROP_NDIS_MINIPORT_GUID L"MiniportGuid" // Network interface GUID (GUID)
#define PROP_NDIS_DIRECTION     L"Direction"   // 0=inbound, 1=outbound (uint32)
#define PROP_NDIS_FRAG_SIZE     L"FragmentSize" // Captured fragment size (uint32)
#define PROP_NDIS_METADATA      L"Metadata"    // Additional packet metadata blob (byte array)


// ============================================================================
// SECTION 6 — Protocol Classification Logic Reference
//
// Algorithm for determining NetworkProtocol from ETW event streams:
//
//  1. gRPC:        ContentType == "application/grpc"  [WebIO/WinINet]
//  2. WebSocket:   URL starts with ws:// or wss://    [WebIO]
//                  OR UpgradeHeader == "websocket"     [WebIO]
//                  OR WebIO frame opcode events (Text/Binary/Ping/Pong)
//  3. REST API:    HTTP method (GET/POST/PUT/DELETE/PATCH)
//                  AND (ContentType contains json/xml/urlencoded
//                       OR URL path contains /api/ or /rest/)
//  4. HTTP/2:      ALPN result == "h2"                [WebIO]
//  5. HTTP/1.1:    ALPN result == "http/1.1"          [WebIO]
//  6. TLS/SSL:     RemotePort == 443 at TCP layer     [TCPIP]
//                  AND Client Hello observed (payload magic bytes 0x16, 0x03)
//  7. DNS:         RemotePort == 53 or LocalPort == 53 [TCPIP]
//  8. RAW_UDP:     UDP event ID (42/43/58/59)         [TCPIP]
//  9. RAW_TCP:     Fallback for all remaining TCP      [TCPIP]
//
// Dynamic Transitions (5-tuple state machine):
//   RAW_TCP → TLS (when port 443 + TLS handshake observed)
//   RAW_TCP → REST_API (when first HTTP method event seen)
//   REST_API → WEBSOCKET (when Upgrade: websocket header sent)
//   WEBSOCKET is terminal (no further transitions)
//   GRPC is terminal (no further transitions)
// ============================================================================


// ============================================================================
// SECTION 7 — Human-Readable Data Volume Formatting Reference
//
//  Formatting rules for formatBytes(quint64 bytes):
//    ≥ 1,073,741,824  (1 GiB)  → display as "X.XX GB"
//    ≥ 1,048,576      (1 MiB)  → display as "X.XX MB"
//    ≥ 102.4          (0.1 KiB)→ display as "X.X KB"
//    <  102.4                  → display as "N Bytes"
//
//  Examples:
//    1,500,000 bytes  → "1.43 MB"
//    512,000   bytes  → "500.0 KB"
//    800       bytes  → "800 Bytes"
// ============================================================================


// ============================================================================
// SECTION 8 — Connection Consolidation Rules Reference
//
// Merging criterion: Two consecutive TCP events for the same PID session
// are consolidated into one ConnectionRecord if and only if:
//   event.localIp == lastRecord.localIp  (exact string match)
//   AND
//   event.remoteIp == lastRecord.remoteIp (exact string match)
//
// On merge:
//   - Append localPort to lastRecord.localPorts  (if not already present)
//   - Append remotePort to lastRecord.remotePorts (if not already present)
//   - Accumulate bytesSent / bytesReceived
//   - Increment connectionCount
//   - Update lastSeen timestamp
//
// On new record (no match):
//   - Initialize new ConnectionRecord with first port, bytes, timestamps
//   - Classify protocol via ProtocolClassifier::classify()
//   - Initiate async DNS reverse lookup for non-private remote IPs
// ============================================================================

#endif // ETW_NETWORK_TAXONOMY_H
