## Requirement: Automatic UAC Administrator Elevation
The system SHALL request Administrator privileges on launch using a Windows manifest. If UAC is declined, execution is blocked.

#### Scenario: Elevated execution
- **WHEN** the application starts
- **THEN** the OS SHALL show the UAC elevation prompt. If accepted, the application starts and displays "System Admin Privileges" on the status bar.

## Requirement: Precise ETW Subscriptions
The system SHALL subscribe to the correct `Microsoft-Windows-TCPIP` (`{2F07E2EE-15DB-40F1-90EF-9D7BA282188A}`) and related provider GUIDs in `EtwTraceSession`.

#### Scenario: Receive connection events
- **WHEN** network TCP/IP events occur
- **THEN** the ETW callbacks SHALL be triggered and capture connection data successfully.
