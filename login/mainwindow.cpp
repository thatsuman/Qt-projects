#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Set password echo mode
    ui->password_field->setEchoMode(QLineEdit::Password);

    // Initialize activity timer
    activityTimer = new QTimer(this);
    activityTimer->setInterval(1000); // 5 seconds
    connect(activityTimer, SIGNAL(timeout()), this, SLOT(logActivity()));

    // Initialize logging state
    isLogging = false;

    // Connect signals and slots
    connect(ui->okPushButton,SIGNAL(clicked()),this,SLOT(okButton())); // push button
    connect(ui->exitPushButton,SIGNAL(clicked()),this,SLOT(close())); // exit button
    connect(ui->pushButton_eye,SIGNAL(clicked()),this,SLOT(togglePasswordVisibility())); // toggle password visibility
    connect(ui->logoutPushButton,SIGNAL(clicked()),this,SLOT(logoutButton())); // logout button
}

MainWindow::~MainWindow()
{
    delete ui;
}

// Ok Push Button
void MainWindow::okButton()
{
    QStringList name = {"jack", "john", "recon"};
    QStringList pass = {"jack123", "john123", "recon123"};

    QString uName = ui->username_field->text();
    QString uPass = ui->password_field->text();

    bool found = false;

    // Empty fields check
    if(uName.isEmpty() || uPass.isEmpty()){
        QMessageBox::information(this,"Missing Field","Please enter username and password");
        return;
    }


    for (int i = 0; i < name.size(); i++){
        if(uName == name[i] && uPass == pass[i]){

            QMessageBox::information(this,"Welcome Message", "User: " + name[i] + "\n"
                                    "Welcome to system");

            // Start activity logging
            currentUser = name[i];
            isLogging = true;

            // Test file writing immediately
            QString testFileName = QString("activity_log_%1.txt").arg(currentUser);
            QFile testFile(testFileName);
            if (testFile.open(QIODevice::Append | QIODevice::Text)) {
                QTextStream out(&testFile);
                out << "=== Logging started at " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << " ===\n";
                testFile.close();
            }

            activityTimer->start();

            // Minimize window and show logout button
            showMinimized();
            ui->logoutPushButton->show();

            // Hide login form elements
            ui->username_field->hide();
            ui->password_field->hide();
            ui->pushButton_eye->hide();
            ui->okPushButton->hide();
            ui->exitPushButton->hide();
            ui->username->hide();
            ui->password->hide();
            ui->label->hide();

            found = true;
            break;
        }
    }
    if(!found){
        QMessageBox::information(this,"Error!","Invalid username or password");
    }
}

// Toggle password visibility
void MainWindow::togglePasswordVisibility()
{
    if(ui->password_field->echoMode() == QLineEdit::Password){
        ui->password_field->setEchoMode(QLineEdit::Normal);
        ui->pushButton_eye->setText("Hide");
    }
    else{
        ui->password_field->setEchoMode(QLineEdit::Password);
        ui->pushButton_eye->setText("Show");
    }
}

// Log user activity
void MainWindow::logActivity()
{
    if (!isLogging) return;

    try {
        // Get foreground window handle
        HWND hwnd = GetForegroundWindow();
        if (hwnd == NULL) return;

        // Get window title
        wchar_t windowTitle[256];
        GetWindowText(hwnd, windowTitle, 256);
        QString title = QString::fromWCharArray(windowTitle);

        // Get process ID
        DWORD processId;
        GetWindowThreadProcessId(hwnd, &processId);

        // Get process name
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        QString processName = "unknown";
        if (hProcess != NULL) {
            wchar_t processNameBuffer[256];
            if (GetModuleBaseName(hProcess, NULL, processNameBuffer, 256) > 0) {
                processName = QString::fromWCharArray(processNameBuffer);
            }
            CloseHandle(hProcess);
        }

        // Format log entry
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        QString logEntry = QString("%1 | %2 | %3 | %4\n")
                               .arg(timestamp)
                               .arg(title)
                               .arg(processName)
                               .arg(currentUser);

        // Write to log file
        QString logFileName = QString("activity_log_%1.txt").arg(currentUser);
        QFile logFile(logFileName);
        if (logFile.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&logFile);
            out << logEntry;
            logFile.close();
        } else {
            // Log file write error - could log to separate error file
            QFile errorFile("activity_error.txt");
            if (errorFile.open(QIODevice::Append | QIODevice::Text)) {
                QTextStream err(&errorFile);
                err << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                    << " - Failed to open log file for user: " << currentUser << "\n";
                errorFile.close();
            }
        }
    } catch (...) {
        // Handle any unexpected errors
        QFile errorFile("activity_error.txt");
        if (errorFile.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream err(&errorFile);
            err << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                << " - Exception in logActivity for user: " << currentUser << "\n";
            errorFile.close();
        }
    }
}

// Logout button
void MainWindow::logoutButton()
{
    // Stop activity logging
    activityTimer->stop();
    isLogging = false;
    currentUser.clear();

    // Restore window to normal state
    showNormal();

    // Hide logout button
    ui->logoutPushButton->hide();

    // Show login form elements
    ui->username_field->show();
    ui->password_field->show();
    ui->pushButton_eye->show();
    ui->okPushButton->show();
    ui->exitPushButton->show();
    ui->username->show();
    ui->password->show();
    ui->label->show();

    // Clear input fields
    ui->username_field->clear();
    ui->password_field->clear();

    QMessageBox::information(this, "Logout", "You have been logged out successfully");
}

// Handle window close event
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (isLogging) {
        activityTimer->stop();
        isLogging = false;
    }
    QMainWindow::closeEvent(event);
}
