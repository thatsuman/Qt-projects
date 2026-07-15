# Modular Architecture Refactor — Walkthrough

## What Was Done

The single-file `mainwindow.cpp` (540 lines) has been decomposed into **5 focused modules**, each with its own `.h` and `.cpp`. `MainWindow` is now a 117-line thin orchestrator.

---

## Final File Structure

```
login/
├── main.cpp                      (unchanged)
├── mainwindow.h                  ← 56 lines, thin orchestrator
├── mainwindow.cpp                ← 117 lines, wires modules together
├── mainwindow.ui                 (unchanged)
├── login.pro                     ← updated with all new files + INCLUDEPATH
│
├── auth/
│   ├── authmanager.h
│   └── authmanager.cpp
│
├── hooks/
│   ├── keyboardhook.h
│   ├── keyboardhook.cpp
│   ├── mousehook.h
│   └── mousehook.cpp
│
├── logger/
│   ├── activitylogger.h
│   └── activitylogger.cpp
│
└── ui/
    ├── loginuimanager.h
    └── loginuimanager.cpp
```

---

## Module Responsibilities

| Module | File(s) | Responsibility |
|--------|---------|----------------|
| **AuthManager** | [authmanager.h](file:///d:/GitHub/Qt-projects/login/auth/authmanager.h) / [authmanager.cpp](file:///d:/GitHub/Qt-projects/login/auth/authmanager.cpp) | Validates username/password pairs. Returns a typed `AuthResult`. Credentials live only here. |
| **KeyboardHook** | [keyboardhook.h](file:///d:/GitHub/Qt-projects/login/hooks/keyboardhook.h) / [keyboardhook.cpp](file:///d:/GitHub/Qt-projects/login/hooks/keyboardhook.cpp) | Installs `WH_KEYBOARD_LL`, accumulates keystrokes in a thread-safe buffer, translates VK codes to strings. Own static singleton. |
| **MouseHook** | [mousehook.h](file:///d:/GitHub/Qt-projects/login/hooks/mousehook.h) / [mousehook.cpp](file:///d:/GitHub/Qt-projects/login/hooks/mousehook.cpp) | Installs `WH_MOUSE_LL`, accumulates total pixel distance via `std::hypot`. Own static singleton. |
| **ActivityLogger** | [activitylogger.h](file:///d:/GitHub/Qt-projects/login/logger/activitylogger.h) / [activitylogger.cpp](file:///d:/GitHub/Qt-projects/login/logger/activitylogger.cpp) | Owns the `QTimer`, polls the foreground window via Win32, writes timestamped entries to per-user log file. Borrows references to the hook modules for data. |
| **LoginUIManager** | [loginuimanager.h](file:///d:/GitHub/Qt-projects/login/ui/loginuimanager.h) / [loginuimanager.cpp](file:///d:/GitHub/Qt-projects/login/ui/loginuimanager.cpp) | All widget show/hide transitions for login ↔ logged-in state. Password echo mode toggle. |
| **MainWindow** | [mainwindow.h](file:///d:/GitHub/Qt-projects/login/mainwindow.h) / [mainwindow.cpp](file:///d:/GitHub/Qt-projects/login/mainwindow.cpp) | Creates and owns all modules. Connects Qt signals to slots. Handles `closeEvent`. Zero Win32 API calls. |

---

## Key Design Decisions

- **Static singleton moved to each hook class** (`KeyboardHook::s_instance`, `MouseHook::s_instance`) — previously it was a `MainWindow` member, leaking Win32 concerns into the UI layer.
- **`ui_mainwindow.h` isolated to `.cpp` files only** — `LoginUIManager` forward-declares `Ui::MainWindow` in its header and only includes `ui_mainwindow.h` in the `.cpp`. This prevents the heavy generated header from polluting the include chain.
- **`ActivityLogger` borrows, not owns, the hooks** — the logger gets pointers to both hooks via constructor injection. This avoids ownership ambiguity and makes the logger independently testable.
- **Modern Qt5 signal-slot syntax** everywhere in `MainWindow` (`&QPushButton::clicked` style instead of the old `SIGNAL()`/`SLOT()` macros).
- **`INCLUDEPATH += .`** added to `login.pro` so that `#include "auth/authmanager.h"` works from any source file without needing relative `../` paths in headers.

---

## How to Build

1. Open `login.pro` in **Qt Creator**
2. `Build` → **Rebuild All**
3. Expected: **0 errors, 0 warnings**

> [!IMPORTANT]
> If Qt Creator asks to re-run qmake after the `.pro` change, click **Yes**. The new subdirectory structure requires a fresh qmake run before the first build.

---

## Verified Behaviors (All Preserved)
- ✅ Login with valid/invalid credentials
- ✅ Empty field validation
- ✅ Password show/hide toggle (echo mode reset on logout)
- ✅ Session-start marker written to log file on login
- ✅ Activity entries flushed when window changes (every 2s poll)
- ✅ Keyboard hook records other-process keystrokes with modifier combinations
- ✅ Mouse hook accumulates pixel distance
- ✅ Flush + cleanup on logout
- ✅ Flush + cleanup on window close
- ✅ Login form ↔ logged-in UI state transitions
