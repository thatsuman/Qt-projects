#include "activitylogger.h"
#include "../hooks/keyboardhook.h"
#include "../hooks/mousehook.h"

#include <QFile>
#include <QTextStream>

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

    // Write a session-start marker to the log file
    QString logFileName = QString("activity_log_%1.txt").arg(m_currentUser);
    QFile logFile(logFileName);
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        out << "=== Logging started at "
            << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
            << " ===\n";
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
    if (!m_active || m_lastWindowTitle.isEmpty()) return;

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
        if (m_lastWindowTitle.isEmpty()) {
            m_lastWindowTitle   = title;
            m_lastProcessName   = processName;
            m_activityStartTime = now;
            return;
        }

        // Same window — keep timing
        if (m_lastWindowTitle == title && m_lastProcessName == processName) {
            return;
        }

        // Window changed — flush the previous activity entry
        QString keystrokes = m_kbHook   ? m_kbHook->getAndClearBuffer()    : QString();
        double  pixels     = m_mouseHook? m_mouseHook->getAndResetDistance(): 0.0;

        writeEntry(m_activityStartTime, now,
                   m_lastWindowTitle, m_lastProcessName,
                   keystrokes, pixels);

        // Start tracking the new window
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
    QString logFileName = QString("activity_log_%1.txt").arg(m_currentUser);
    QFile logFile(logFileName);

    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        out << from.toString("yyyy-MM-dd hh:mm:ss")
            << " - "
            << to.toString("yyyy-MM-dd hh:mm:ss")
            << " | " << windowTitle
            << " | " << processName
            << " | " << m_currentUser
            << " | \"" << keystrokes << "\""
            << " | Mouse Distance: " << mousePixels << " px\n";
        logFile.close();
    }
}
