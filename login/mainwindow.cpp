#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "auth/authmanager.h"
#include "hooks/keyboardhook.h"
#include "hooks/mousehook.h"
#include "logger/activitylogger.h"
#include "ui/loginuimanager.h"

#include <QMessageBox>
#include <QLineEdit>
#include <QPushButton>

// ── Constructor ───────────────────────────────────────────────────────────────
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_auth(new AuthManager)
    , m_kbHook(new KeyboardHook)
    , m_mouseHook(new MouseHook)
    , m_logger(new ActivityLogger(m_kbHook, m_mouseHook, this))
    , m_uiManager(new LoginUIManager(ui))
{
    ui->setupUi(this);

    // Set password field to hidden by default
    ui->password_field->setEchoMode(QLineEdit::Password);

    // Wire UI signals to our slots
    connect(ui->okPushButton,      &QPushButton::clicked, this, &MainWindow::onLogin);
    connect(ui->exitPushButton,    &QPushButton::clicked, this, &MainWindow::close);
    connect(ui->pushButton_eye,    &QPushButton::clicked, this, &MainWindow::onTogglePasswordVisibility);
    connect(ui->logoutPushButton,  &QPushButton::clicked, this, &MainWindow::onLogout);
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

    // Start activity logger
    m_logger->start(result.username);

    // Transition UI
    m_uiManager->showLoggedInState();
    showMinimized();
}

void MainWindow::onLogout()
{
    // Flush the last activity before removing hooks
    m_logger->flushCurrentActivity();
    m_logger->stop();

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
        m_kbHook->uninstall();
        m_mouseHook->uninstall();
    }
    QMainWindow::closeEvent(event);
}