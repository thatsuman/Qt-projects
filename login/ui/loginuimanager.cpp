#include "loginuimanager.h"
#include "ui_mainwindow.h"
#include <QLineEdit>

LoginUIManager::LoginUIManager(Ui::MainWindow *ui)
    : m_ui(ui)
{
}

void LoginUIManager::showLoggedInState()
{
    // Hide every element that belongs to the login form
    m_ui->username_field->hide();
    m_ui->password_field->hide();
    m_ui->pushButton_eye->hide();
    m_ui->okPushButton->hide();
    m_ui->exitPushButton->hide();
    m_ui->username->hide();
    m_ui->password->hide();
    m_ui->label->hide();

    // Show the logged-in control
    m_ui->logoutPushButton->show();
}

void LoginUIManager::showLoginForm()
{
    // Hide logged-in controls
    m_ui->logoutPushButton->hide();

    // Restore every login-form element
    m_ui->username_field->show();
    m_ui->password_field->show();
    m_ui->pushButton_eye->show();
    m_ui->okPushButton->show();
    m_ui->exitPushButton->show();
    m_ui->username->show();
    m_ui->password->show();
    m_ui->label->show();

    // Clear sensitive input data
    m_ui->username_field->clear();
    m_ui->password_field->clear();

    // Reset password echo mode to hidden
    m_ui->password_field->setEchoMode(QLineEdit::Password);
    m_ui->pushButton_eye->setText("show");
}

void LoginUIManager::togglePasswordVisibility()
{
    if (m_ui->password_field->echoMode() == QLineEdit::Password) {
        m_ui->password_field->setEchoMode(QLineEdit::Normal);
        m_ui->pushButton_eye->setText("Hide");
    } else {
        m_ui->password_field->setEchoMode(QLineEdit::Password);
        m_ui->pushButton_eye->setText("Show");
    }
}
