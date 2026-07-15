#ifndef ACTIVITYLOGGER_H
#define ACTIVITYLOGGER_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QString>
#include <windows.h>
#include <psapi.h>

// Forward declarations — avoids including full headers here
class KeyboardHook;
class MouseHook;

/**
 * @brief ActivityLogger polls the foreground window every 2 seconds and writes
 *        time-stamped activity entries (window title, process name, user,
 *        keystrokes, mouse distance) to a per-user log file.
 *
 * It borrows references to KeyboardHook and MouseHook to pull their data
 * at each log boundary; it does NOT own them.
 *
 * Usage:
 *   ActivityLogger logger(&kbHook, &mouseHook, this);
 *   logger.start("jack");
 *   ...
 *   logger.flushCurrentActivity(); // call before stopping hooks
 *   logger.stop();
 */
class ActivityLogger : public QObject
{
    Q_OBJECT

public:
    explicit ActivityLogger(KeyboardHook *kbHook,
                            MouseHook    *mouseHook,
                            QObject      *parent = nullptr);
    ~ActivityLogger() override;

    /**
     * @brief Begin logging for the given user.
     *        Writes a session-start marker and starts the polling timer.
     */
    void start(const QString &username);

    /**
     * @brief Flush the currently-tracked activity to disk, then stop the timer.
     *        Safe to call even if not active.
     */
    void stop();

    /**
     * @brief Write out the pending activity entry without stopping the logger.
     *        Call this just before removing hooks or on logout/close.
     */
    void flushCurrentActivity();

    /** @brief Returns true if logging is currently active. */
    bool isActive() const;

private slots:
    void onTimer();

private:
    /** Write a single activity log entry to the per-user file. */
    void writeEntry(const QDateTime &from,
                    const QDateTime &to,
                    const QString   &windowTitle,
                    const QString   &processName,
                    const QString   &keystrokes,
                    double           mousePixels);

    QTimer      *m_timer;
    QString      m_currentUser;
    QString      m_lastWindowTitle;
    QString      m_lastProcessName;
    QDateTime    m_activityStartTime;
    bool         m_active;

    KeyboardHook *m_kbHook;
    MouseHook    *m_mouseHook;
};

#endif // ACTIVITYLOGGER_H
