# Proposal: UAC Elevation and ETW Trace Session GUID Fixes

## Why
Real-time network events via ETW were not being captured because the GUID definitions in the code were incorrect for the system's manifest-based ETW providers. Additionally, since the application requires administrator privileges to start real-time trace sessions, it must prompt for UAC elevation on startup, rather than running silently as a standard user and failing.

## What Changes
1.  Update all provider GUIDs in `etw/etw_providers.h` to match the actual Windows system registrations.
2.  Enable verbose tracing (`TRACE_LEVEL_VERBOSE` / 5) and keywords to capture high-frequency send/receive operations.
3.  Configure `login.pro` to embed `requireAdministrator` in the application manifest so it triggers a UAC prompt on launch.
4.  Update `MainWindow` UI to display `"System Admin Privileges"` on the status bar upon successful startup.

## Capabilities
-   `etw-uac-elevation`: Manifest UAC elevation control and status logging.
-   `etw-guid-alignment`: Alignment of ETW provider GUID declarations with actual Windows subsystem GUIDs.
