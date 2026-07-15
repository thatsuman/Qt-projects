# Modular Architecture Refactor — Qt Login Project

## Overview

The current codebase has all logic packed into a single `mainwindow.cpp` / `mainwindow.h` pair (~540 lines). The goal is to extract each responsibility into its own module — each with a clean `.h` interface and a `.cpp` implementation — while keeping `MainWindow` as the thin orchestrator.

---

## Identified Functionality Domains

| Domain | Current Location | New Module |
|--------|-----------------|------------|
| Authentication / credential check | `okButton()` in mainwindow.cpp | `auth/AuthManager` |
| Activity logging (window tracking) | `logActivity()`, `closeEvent()`, `logoutButton()` | `logger/ActivityLogger` |
| Keyboard hook + key-name resolution | `keyboardHookCallback()`, `getKeyName()` | `hooks/KeyboardHook` |
| Mouse hook + distance tracking | `mouseHookCallback()` | `hooks/MouseHook` |
| UI state management (show/hide widgets on login/logout) | scattered in okButton/logoutButton | `ui/LoginUIManager` |
| Main window orchestration | `MainWindow` | stays as `mainwindow` — now thin |

---

## Proposed File Structure

```
login/
├── main.cpp                      (unchanged)
├── mainwindow.h                  (MODIFY — slimmed down)
├── mainwindow.cpp                (MODIFY — thin orchestrator only)
├── mainwindow.ui                 (unchanged)
├── login.pro                     (MODIFY — add new source files)
│
├── auth/
│   ├── authmanager.h             [NEW]
│   └── authmanager.cpp           [NEW]
│
├── logger/
│   ├── activitylogger.h          [NEW]
│   └── activitylogger.cpp        [NEW]
│
├── hooks/
│   ├── keyboardhook.h            [NEW]
│   ├── keyboardhook.cpp          [NEW]
│   ├── mousehook.h               [NEW]
│   └── mousehook.cpp             [NEW]
│
└── ui/
    ├── loginuimanager.h          [NEW]
    └── loginuimanager.cpp        [NEW]
```

---

## Module Details

### `auth/AuthManager`
- **Responsibility**: Validate username/password pairs. Returns a result (success + matched username, or failure). Credentials are defined here — easy to swap for a DB or config file later.
- **Interface**: `AuthResult authenticate(const QString& username, const QString& password)`

### `logger/ActivityLogger`
- **Responsibility**: Write time-stamped activity entries (window title, process name, user, keystrokes, mouse distance) to a per-user log file. Owns the `QTimer`, foreground-window polling (Windows API), and log-file I/O. Emits a `logSessionStarted()` / `logSessionEnded()` signal.
- **Interface**: `start(username)`, `stop()`, `flushCurrentActivity()`, `setKeystrokeBuffer(...)`, `setMouseDistance(...)`

### `hooks/KeyboardHook`
- **Responsibility**: Install/uninstall `WH_KEYBOARD_LL`, accumulate keystroke text into a thread-safe buffer. Exposes the buffer via `getAndClearBuffer()`.
- **Interface**: `install()`, `uninstall()`, `getAndClearBuffer() -> QString`

### `hooks/MouseHook`
- **Responsibility**: Install/uninstall `WH_MOUSE_LL`, accumulate pixel-distance traveled. Exposes distance via `getAndResetDistance()`.
- **Interface**: `install()`, `uninstall()`, `getAndResetDistance() -> double`

### `ui/LoginUIManager`
- **Responsibility**: Owns references to all the UI widgets relevant to login/logout transitions. Provides `showLoginForm()` and `showLoggedInState()` to toggle visibility in one clean call. Also contains `togglePasswordVisibility()`.
- **Interface**: `showLoginForm()`, `showLoggedInState()`, `togglePasswordVisibility()`

### `mainwindow` (thin orchestrator)
- **Responsibility**: Wire up signals/slots, create module instances, forward events to the right module. Should be <100 lines of logic.

---

## Proposed Changes

### Build System

#### [MODIFY] [login.pro](file:///d:/GitHub/Qt-projects/login/login.pro)
Add all new `.cpp` and `.h` files to `SOURCES` and `HEADERS`, add `INCLUDEPATH += .` so subdirectory includes work cleanly.

---

### Auth Module

#### [NEW] [authmanager.h](file:///d:/GitHub/Qt-projects/login/auth/authmanager.h)
#### [NEW] [authmanager.cpp](file:///d:/GitHub/Qt-projects/login/auth/authmanager.cpp)

---

### Logger Module

#### [NEW] [activitylogger.h](file:///d:/GitHub/Qt-projects/login/logger/activitylogger.h)
#### [NEW] [activitylogger.cpp](file:///d:/GitHub/Qt-projects/login/logger/activitylogger.cpp)

---

### Hooks Module

#### [NEW] [keyboardhook.h](file:///d:/GitHub/Qt-projects/login/hooks/keyboardhook.h)
#### [NEW] [keyboardhook.cpp](file:///d:/GitHub/Qt-projects/login/hooks/keyboardhook.cpp)
#### [NEW] [mousehook.h](file:///d:/GitHub/Qt-projects/login/hooks/mousehook.h)
#### [NEW] [mousehook.cpp](file:///d:/GitHub/Qt-projects/login/hooks/mousehook.cpp)

---

### UI Manager Module

#### [NEW] [loginuimanager.h](file:///d:/GitHub/Qt-projects/login/ui/loginuimanager.h)
#### [NEW] [loginuimanager.cpp](file:///d:/GitHub/Qt-projects/login/ui/loginuimanager.cpp)

---

### Core Window

#### [MODIFY] [mainwindow.h](file:///d:/GitHub/Qt-projects/login/mainwindow.h)
#### [MODIFY] [mainwindow.cpp](file:///d:/GitHub/Qt-projects/login/mainwindow.cpp)

---

## Verification Plan

### Build Verification
- Open project in Qt Creator and do a clean build (`Build > Rebuild All`) — must produce 0 errors, 0 warnings.
- All existing functionality must work identically:
  - Login with valid/invalid credentials
  - Password show/hide toggle
  - Activity logging starts/stops correctly
  - Keyboard & mouse hooks install and uninstall cleanly
  - Logout restores login form
  - Close event flushes final log entry

### Code Quality Checks
- Each module `.h` is self-contained (uses forward declarations where possible).
- No circular includes.
- `MainWindow` has no direct Windows API calls — all delegated to modules.

> [!IMPORTANT]
> The `.ui` file and `main.cpp` will **not** be changed. The `.pro` file will be updated to include the new sources.

> [!NOTE]
> The static singleton pattern used by Windows hook callbacks (`MainWindow::instance`) will be moved into each hook class (`KeyboardHook::instance`, `MouseHook::instance`) — this is the correct encapsulation.
