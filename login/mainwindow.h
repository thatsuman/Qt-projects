#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCloseEvent>

// Forward declarations — keep this header light
class AuthManager;
class ActivityLogger;
class KeyboardHook;
class MouseHook;
class LoginUIManager;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * @brief MainWindow is the thin orchestrator that wires together all
 *        functional modules: AuthManager, ActivityLogger, KeyboardHook,
 *        MouseHook, and LoginUIManager.
 *
 * It owns every module object and is responsible for:
 *  - Connecting UI signals to the appropriate module methods.
 *  - Handling Qt lifecycle events (close, show).
 *  - No direct Windows API calls — all delegated to modules.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onLogin();
    void onLogout();
    void onTogglePasswordVisibility();

private:
    Ui::MainWindow *ui;

    // Functional modules — owned by MainWindow
    AuthManager     *m_auth;
    KeyboardHook    *m_kbHook;
    MouseHook       *m_mouseHook;
    ActivityLogger  *m_logger;
    LoginUIManager  *m_uiManager;
};

#endif // MAINWINDOW_H
