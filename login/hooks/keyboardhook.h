#ifndef KEYBOARDHOOK_H
#define KEYBOARDHOOK_H

#include <windows.h>
#include <QString>
#include <QMutex>

/**
 * @brief KeyboardHook installs a low-level Windows keyboard hook (WH_KEYBOARD_LL)
 *        and accumulates keystrokes into a thread-safe buffer.
 *
 * Usage:
 *   KeyboardHook kbHook;
 *   kbHook.install();
 *   ...
 *   QString keys = kbHook.getAndClearBuffer();
 *   ...
 *   kbHook.uninstall();
 */
class KeyboardHook
{
public:
    KeyboardHook();
    ~KeyboardHook();

    /** @brief Install the low-level keyboard hook. Returns true on success. */
    bool install();

    /** @brief Uninstall the keyboard hook if it is currently installed. */
    void uninstall();

    /** @brief Returns accumulated keystrokes and clears the internal buffer. Thread-safe. */
    QString getAndClearBuffer();

    /** @brief Returns true if the hook is currently installed. */
    bool isInstalled() const;

private:
    HHOOK   m_hook;
    QString m_buffer;
    QMutex  m_mutex;

    // Singleton pointer required by the static Win32 callback
    static KeyboardHook *s_instance;

    static LRESULT CALLBACK hookCallback(int nCode, WPARAM wParam, LPARAM lParam);

    /**
     * @brief Convert a virtual-key code to a human-readable string.
     * @param vkCode Virtual key code from KBDLLHOOKSTRUCT.
     * @return Printable key representation, or empty string if not applicable.
     */
    static QString keyName(int vkCode);
};

#endif // KEYBOARDHOOK_H
