#ifndef MOUSEHOOK_H
#define MOUSEHOOK_H

#include <windows.h>
#include <QMutex>

/**
 * @brief MouseHook installs a low-level Windows mouse hook (WH_MOUSE_LL)
 *        and accumulates the total pixel distance the mouse has travelled.
 *
 * Usage:
 *   MouseHook mouseHook;
 *   mouseHook.install();
 *   ...
 *   double pixels = mouseHook.getAndResetDistance();
 *   ...
 *   mouseHook.uninstall();
 */
class MouseHook
{
public:
    MouseHook();
    ~MouseHook();

    /** @brief Install the low-level mouse hook. Returns true on success. */
    bool install();

    /** @brief Uninstall the mouse hook if it is currently installed. */
    void uninstall();

    /** @brief Returns total accumulated mouse distance (pixels) and resets to 0. Thread-safe. */
    double getAndResetDistance();

    /** @brief Returns true if the hook is currently installed. */
    bool isInstalled() const;

private:
    HHOOK  m_hook;
    POINT  m_lastPoint;
    bool   m_hasLastPoint;
    double m_distance;
    QMutex m_mutex;

    // Singleton pointer required by the static Win32 callback
    static MouseHook *s_instance;

    static LRESULT CALLBACK hookCallback(int nCode, WPARAM wParam, LPARAM lParam);
};

#endif // MOUSEHOOK_H
