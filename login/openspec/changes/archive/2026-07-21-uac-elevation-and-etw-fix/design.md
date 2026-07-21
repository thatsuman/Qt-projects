# Design: UAC Elevation and ETW Trace Session GUID Fixes

## Context
Event Tracing for Windows (ETW) requires elevated Administrator privileges to execute `StartTrace`. If executed as a standard user, it fails.
Currently, incorrect GUID declarations prevent events from being matched even when running elevated.

## Decisions

### 1. GUID Correction
The correct system GUIDs for the providers are:
*   `Microsoft-Windows-TCPIP`: `{2F07E2EE-15DB-40F1-90EF-9D7BA282188A}`
*   `Microsoft-Windows-WebIO`: `{50B3E73C-9370-461D-BB9F-26F32D68887D}`
*   `Microsoft-Windows-WinINet`: `{43D1A55C-76D6-4F7E-995C-64C711E5CAFE}`
*   `Microsoft-Windows-DNS-Client`: `{1C95126E-7EEA-49A9-A3FE-A378B03DDB4D}`
*   `Microsoft-Windows-NDIS-PacketCapture`: `{2ED6006E-4729-4609-B423-3EE7BCD678EF}`

### 2. Manifest UAC Configuration
Using MSVC compiler flags in `login.pro` to embed UAC level `requireAdministrator`:
```qmake
win32-msvc* {
    QMAKE_LFLAGS += /MANIFESTUAC:\"level=\'requireAdministrator\' uiAccess=\'false\'\"
}
```

### 3. UI Status Integration
Access `ui->statusbar` in `MainWindow`'s constructor and display the privilege status.
