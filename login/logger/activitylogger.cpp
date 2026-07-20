#include "activitylogger.h"
#include "../hooks/keyboardhook.h"
#include "../hooks/mousehook.h"

#include <QFile>
#include <QTextStream>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDir>

// ── Constructor / Destructor ──────────────────────────────────────────────────
ActivityLogger::ActivityLogger(KeyboardHook *kbHook,
                               MouseHook    *mouseHook,
                               QObject      *parent)
    : QObject(parent)
    , m_active(false)
    , m_kbHook(kbHook)
    , m_mouseHook(mouseHook)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(2000); // poll every 2 seconds
    connect(m_timer, &QTimer::timeout, this, &ActivityLogger::onTimer);
}

ActivityLogger::~ActivityLogger()
{
    stop();
}

// ── Public API ────────────────────────────────────────────────────────────────
void ActivityLogger::start(const QString &username)
{
    m_currentUser      = username;
    m_lastWindowTitle.clear();
    m_lastProcessName.clear();
    m_activityStartTime = QDateTime();
    m_active            = true;

    // TASK 6.2: create per-user log directory
    QString userLogDir = QString("logs/%1").arg(m_currentUser);
    QDir().mkpath(userLogDir);

    // Write a session-start marker to the log file in JSON Lines format
    QString logFileName = QString("%1/activity_log.jsonl").arg(userLogDir);
    QFile logFile(logFileName);
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QJsonObject sessionStartObj;
        sessionStartObj["type"] = "session_start";
        sessionStartObj["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        sessionStartObj["username"] = m_currentUser;

        QJsonDocument doc(sessionStartObj);
        QByteArray bytes = doc.toJson(QJsonDocument::Compact);

        QTextStream out(&logFile);
        out << bytes << "\n";
        logFile.close();
    }

    m_timer->start();
}

void ActivityLogger::stop()
{
    if (!m_active) return;
    flushCurrentActivity();
    m_timer->stop();
    m_active = false;
    m_currentUser.clear();
}

void ActivityLogger::flushCurrentActivity()
{
    if (!m_active || m_lastProcessName.isEmpty()) return;

    QDateTime now = QDateTime::currentDateTime();

    QString keystrokes = m_kbHook   ? m_kbHook->getAndClearBuffer()    : QString();
    double  pixels     = m_mouseHook? m_mouseHook->getAndResetDistance(): 0.0;

    writeEntry(m_activityStartTime, now,
               m_lastWindowTitle, m_lastProcessName,
               keystrokes, pixels);
}

bool ActivityLogger::isActive() const
{
    return m_active;
}

// ── Private slot ──────────────────────────────────────────────────────────────
void ActivityLogger::onTimer()
{
    if (!m_active) return;

    try {
        // --- Get foreground window ---
        HWND hwnd = GetForegroundWindow();
        if (hwnd == NULL) return;

        // Window title
        wchar_t titleBuf[256];
        GetWindowText(hwnd, titleBuf, 256);
        QString title = QString::fromWCharArray(titleBuf);

        // Process name
        DWORD processId = 0;
        GetWindowThreadProcessId(hwnd, &processId);

        QString processName = "unknown";
        HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        if (hProc != NULL) {
            wchar_t procBuf[256];
            if (GetModuleBaseName(hProc, NULL, procBuf, 256) > 0) {
                processName = QString::fromWCharArray(procBuf);
            }
            CloseHandle(hProc);
        }

        QDateTime now = QDateTime::currentDateTime();

        // First observation — just record it and wait
        if (m_lastProcessName.isEmpty()) {
            m_lastWindowTitle   = title;
            m_lastProcessName   = processName;
            m_activityStartTime = now;
            return;
        }

        // Same process/application — keep timing and update the latest window title
        if (m_lastProcessName == processName) {
            m_lastWindowTitle = title;
            return;
        }

        // Process changed — flush the previous activity entry
        QString keystrokes = m_kbHook   ? m_kbHook->getAndClearBuffer()    : QString();
        double  pixels     = m_mouseHook? m_mouseHook->getAndResetDistance(): 0.0;

        writeEntry(m_activityStartTime, now,
                   m_lastWindowTitle, m_lastProcessName,
                   keystrokes, pixels);

        // Start tracking the new process
        m_lastWindowTitle   = title;
        m_lastProcessName   = processName;
        m_activityStartTime = now;

    } catch (...) {
        // Safety net — write a minimal error record
        QFile errFile("activity_error.txt");
        if (errFile.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream err(&errFile);
            err << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                << " - Exception in ActivityLogger::onTimer for user: "
                << m_currentUser << "\n";
            errFile.close();
        }
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────
void ActivityLogger::writeEntry(const QDateTime &from,
                                const QDateTime &to,
                                const QString   &windowTitle,
                                const QString   &processName,
                                const QString   &keystrokes,
                                double           mousePixels)
{
    QString logFileName = QString("logs/%1/activity_log.jsonl").arg(m_currentUser);
    QFile logFile(logFileName);

    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QJsonObject activityObj;
        activityObj["type"] = "activity";
        activityObj["timestamp_start"] = from.toString("yyyy-MM-dd hh:mm:ss");
        activityObj["timestamp_end"] = to.toString("yyyy-MM-dd hh:mm:ss");
        activityObj["window_title"] = windowTitle;
        activityObj["process_name"] = processName;
        activityObj["username"] = m_currentUser;
        activityObj["keystrokes"] = keystrokes;
        activityObj["mouse_distance_px"] = mousePixels;

        QJsonDocument doc(activityObj);
        QByteArray bytes = doc.toJson(QJsonDocument::Compact);

        QTextStream out(&logFile);
        out << bytes << "\n";
        logFile.close();
    }
}
