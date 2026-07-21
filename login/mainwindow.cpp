#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "auth/authmanager.h"
#include "hooks/keyboardhook.h"
#include "hooks/mousehook.h"
#include "logger/activitylogger.h"
#include "logger/networklogger.h"
#include "ui/loginuimanager.h"
#include "etw/EtwTraceSession.h"

#include <QMessageBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>

// ── Constructor ───────────────────────────────────────────────────────────────
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_auth(new AuthManager)
    , m_kbHook(new KeyboardHook)
    , m_mouseHook(new MouseHook)
    , m_logger(new ActivityLogger(m_kbHook, m_mouseHook, this))
    , m_netLogger(new NetworkLogger(this))
    , m_uiManager(new LoginUIManager(ui))
    , m_telemetryView(new QTextEdit(this))
{
    ui->setupUi(this);

    // Check UAC privileges and show status in the status bar
    if (EtwTraceSession::isUserAdminOrPerformanceLogUser()) {
        ui->statusbar->showMessage("System Admin Privileges");
    } else {
        ui->statusbar->showMessage("Standard User Privileges (ETW Disabled)");
    }

    // Set password field to hidden by default
    ui->password_field->setEchoMode(QLineEdit::Password);

    // Setup network telemetry view
    m_telemetryView->setReadOnly(true);
    m_telemetryView->setGeometry(300, 100, 560, 250);
    m_telemetryView->setStyleSheet("QTextEdit { background-color: #1e1e1e; color: #00ff00; font-family: 'Consolas'; font-size: 11px; border: 1px solid #333; }");
    m_telemetryView->hide();

    // Wire UI signals to our slots
    connect(ui->okPushButton,      &QPushButton::clicked, this, &MainWindow::onLogin);
    connect(ui->exitPushButton,    &QPushButton::clicked, this, &MainWindow::close);
    connect(ui->pushButton_eye,    &QPushButton::clicked, this, &MainWindow::onTogglePasswordVisibility);
    connect(ui->logoutPushButton,  &QPushButton::clicked, this, &MainWindow::onLogout);

    connect(m_netLogger, &NetworkLogger::networkActivityOccurred, this, [this](const QString &logText) {
        m_telemetryView->append(logText);
    });
}

// ── Destructor ────────────────────────────────────────────────────────────────
MainWindow::~MainWindow()
{
    // Modules owned by `this` (as QObject children) are cleaned up automatically.
    // Non-QObject modules must be deleted explicitly.
    delete m_uiManager;
    delete m_mouseHook;
    delete m_kbHook;
    delete m_auth;
    delete ui;
}

// ── Slots ─────────────────────────────────────────────────────────────────────
void MainWindow::onLogin()
{
    const QString uName = ui->username_field->text();
    const QString uPass = ui->password_field->text();

    if (uName.isEmpty() || uPass.isEmpty()) {
        QMessageBox::information(this, "Missing Field",
                                 "Please enter username and password");
        return;
    }

    const AuthResult result = m_auth->authenticate(uName, uPass);

    if (!result.success) {
        QMessageBox::information(this, "Error!", "Invalid username or password");
        return;
    }

    // ── Successful login ──────────────────────────────────────────────────
    QMessageBox::information(this, "Welcome Message",
                             "User: " + result.username + "\nWelcome to system");

    // Start hooks
    if (!m_kbHook->install()) {
        QMessageBox::warning(this, "Warning", "Failed to install keyboard hook");
    }
    m_mouseHook->install();

    // Start activity logger and network logger
    m_logger->start(result.username);
    m_netLogger->start(result.username);

    // Show telemetry view
    m_telemetryView->show();
    m_telemetryView->clear();
    m_telemetryView->append("--- Real-time ETW Network Telemetry Feed (Running with Administrator privileges) ---");

    // Transition UI
    m_uiManager->showLoggedInState();
    showMinimized();
}

void MainWindow::onLogout()
{
    // Flush the last activity before removing hooks
    m_logger->flushCurrentActivity();
    m_logger->stop();
    m_netLogger->stop();

    m_telemetryView->hide();

    m_kbHook->uninstall();
    m_mouseHook->uninstall();

    // Restore window and login form
    showNormal();
    m_uiManager->showLoginForm();

    QMessageBox::information(this, "Logout", "You have been logged out successfully");
}

void MainWindow::onTogglePasswordVisibility()
{
    m_uiManager->togglePasswordVisibility();
}

// ── Event override ────────────────────────────────────────────────────────────
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_logger->isActive()) {
        // Flush pending activity, then stop logging and hooks cleanly
        m_logger->flushCurrentActivity();
        m_logger->stop();
        m_netLogger->stop();
        m_telemetryView->hide();
        m_kbHook->uninstall();
        m_mouseHook->uninstall();
    }
    QMainWindow::closeEvent(event);
}