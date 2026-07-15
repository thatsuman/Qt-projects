#include "mousehook.h"
#include <cmath>

// ── Static singleton ─────────────────────────────────────────────────────────
MouseHook *MouseHook::s_instance = nullptr;

// ── Constructor / Destructor ──────────────────────────────────────────────────
MouseHook::MouseHook()
    : m_hook(nullptr)
    , m_hasLastPoint(false)
    , m_distance(0.0)
{
    m_lastPoint = {0, 0};
}

MouseHook::~MouseHook()
{
    uninstall();
}

// ── Public API ────────────────────────────────────────────────────────────────
bool MouseHook::install()
{
    if (m_hook != nullptr) return true; // already installed

    m_hasLastPoint = false;
    m_distance     = 0.0;
    s_instance     = this;
    m_hook = SetWindowsHookEx(WH_MOUSE_LL, hookCallback, GetModuleHandle(NULL), 0);
    return (m_hook != nullptr);
}

void MouseHook::uninstall()
{
    if (m_hook != nullptr) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

double MouseHook::getAndResetDistance()
{
    m_mutex.lock();
    double dist = m_distance;
    m_distance  = 0.0;
    m_mutex.unlock();
    return dist;
}

bool MouseHook::isInstalled() const
{
    return (m_hook != nullptr);
}

// ── Static callback ───────────────────────────────────────────────────────────
LRESULT CALLBACK MouseHook::hookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && wParam == WM_MOUSEMOVE && s_instance != nullptr) {
        MSLLHOOKSTRUCT *mouse = reinterpret_cast<MSLLHOOKSTRUCT *>(lParam);

        s_instance->m_mutex.lock();

        if (!s_instance->m_hasLastPoint) {
            s_instance->m_lastPoint    = mouse->pt;
            s_instance->m_hasLastPoint = true;
        } else {
            LONG dx = mouse->pt.x - s_instance->m_lastPoint.x;
            LONG dy = mouse->pt.y - s_instance->m_lastPoint.y;
            s_instance->m_distance += std::hypot(static_cast<double>(dx),
                                                  static_cast<double>(dy));
            s_instance->m_lastPoint = mouse->pt;
        }

        s_instance->m_mutex.unlock();
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
