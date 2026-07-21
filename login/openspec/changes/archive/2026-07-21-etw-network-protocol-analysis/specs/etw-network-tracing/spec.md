## ADDED Requirements

### Requirement: ETW Session Initialization and Provider Subscription
The system SHALL initialize an Event Tracing for Windows (ETW) real-time trace session and subscribe to `Microsoft-Windows-TCPIP` (`{2F07E2EE-15DB-4B1F-B6A0-B6328C2A578C}`), `Microsoft-Windows-WebIO`, `WinINet`, and `Microsoft-Windows-DNS-Client` ETW providers.

#### Scenario: ETW session started with valid privileges
- **WHEN** the user launches the network monitor component with Administrator privileges or Performance Log Users group access
- **THEN** the system SHALL successfully call `StartTraceW` and `EnableTraceEx2` to begin consuming live kernel and user-mode network events.

#### Scenario: ETW session initialization failure due to insufficient privileges
- **WHEN** the application runs under standard non-elevated user permissions
- **THEN** the system SHALL return an explicit error status indicating administrative elevation is required and disable ETW tracing safely.

### Requirement: Real-time Event Consumption and Process Mapping
The system SHALL run an asynchronous event consumer loop via `OpenTraceW` and `ProcessTrace` to extract event records and map each event's Process ID (PID) to process image path and active socket descriptors.

#### Scenario: Real-time connection event capture
- **WHEN** network traffic or TCP/UDP socket state changes occur on the operating system
- **THEN** the system SHALL parse ETW event records and emit structured event structures containing timestamp, PID, process name, local IP, local port, remote IP, remote port, and event type.
