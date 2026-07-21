### Requirement: Application Protocol Classification Engine
The system SHALL classify network connections into explicit protocol categories (REST API, WebSocket, HTTP/1.1, HTTP/2, gRPC, TLS/SSL, DNS, Raw TCP, Raw UDP) using ETW provider metadata, port heuristics, TLS negotiation parameters, and payload header inspection.

#### Scenario: Identify REST API HTTP connections
- **WHEN** ETW events from `WinINet`/`WebIO` or HTTP stream payloads contain HTTP request methods (`GET`, `POST`, `PUT`, `DELETE`, `PATCH`) alongside RESTful path structures or JSON/XML content-types
- **THEN** the system SHALL classify the connection as `REST API`.

#### Scenario: Identify WebSocket connections
- **WHEN** an HTTP connection initiates an `Upgrade: websocket` header exchange or WebIO events record WebSocket frame send/receive activity
- **THEN** the system SHALL classify the connection as `WebSocket` and track bidirectional frame traffic.

#### Scenario: Identify TLS SNI and ALPN protocol extension
- **WHEN** TLS negotiation occurs on a TCP connection
- **THEN** the system SHALL extract the Server Name Indication (SNI) host and Application-Layer Protocol Negotiation (ALPN) string (e.g., `h2`, `http/1.1`) to classify the protocol.

### Requirement: Dynamic Protocol Transition State Tracking
The system SHALL maintain an active connection map indexed by 5-tuple (Protocol, Local IP, Local Port, Remote IP, Remote Port, PID) and update protocol classification as initial handshake payloads are parsed.

#### Scenario: Dynamic protocol upgrade tracking
- **WHEN** a connection initially observed as `TCP` sends an HTTP GET request with `Upgrade: websocket`
- **THEN** the system SHALL transition the connection's classification state from `TCP` -> `REST API` -> `WebSocket` in real time.
