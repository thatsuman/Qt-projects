## 1. ETW GUID Corrections
- [x] 1.1 Update `etw/etw_providers.h` with the correct registered GUIDs for `TCPIP`, `WebIO`, `WinINet`, `DNS-Client`, and `NDIS` providers.
- [x] 1.2 Enable verbose tracing level and all keywords inside `EtwTraceSession::startSession()`.

## 2. UAC Manifest Integration
- [x] 2.1 Update `login.pro` to include MSVC linker flag `/MANIFESTUAC` requiring administrator privileges.

## 3. UI Status Indicator
- [x] 3.1 Update `MainWindow` constructor to show `"System Admin Privileges"` on the status bar.

## 4. Verification
- [x] 4.1 Verify UAC prompt pops up on application launch.
- [x] 4.2 Verify event logging is successfully capturing network traces in `network_logger_debug.txt`.
