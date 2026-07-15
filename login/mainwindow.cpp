#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <cmath>

// Initialize static instance
MainWindow* MainWindow::instance = nullptr;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Set password echo mode
    ui->password_field->setEchoMode(QLineEdit::Password);

    // Initialize activity timer
    activityTimer = new QTimer(this);
    activityTimer->setInterval(2000); // 2 seconds
    connect(activityTimer, SIGNAL(timeout()), this, SLOT(logActivity()));

    // Initialize logging state
    isLogging = false;

    // Initialize keyboard hook
    keyboardHook = nullptr;
    instance = this;

    // Initialize mouse hook
    mouseHook = nullptr;
    mouseDistance = 0.0;
    hasLastMousePoint = false;

    // Connect signals and slots
    connect(ui->okPushButton,SIGNAL(clicked()),this,SLOT(okButton())); // push button
    connect(ui->exitPushButton,SIGNAL(clicked()),this,SLOT(close())); // exit button
    connect(ui->pushButton_eye,SIGNAL(clicked()),this,SLOT(togglePasswordVisibility())); // toggle password visibility
    connect(ui->logoutPushButton,SIGNAL(clicked()),this,SLOT(logoutButton())); // logout button
}

MainWindow::~MainWindow()
{
    if (keyboardHook != nullptr) {
        UnhookWindowsHookEx(keyboardHook);
    }

    if(mouseHook != nullptr){
        UnhookWindowsHookEx(mouseHook);
    }
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

            // Reset them on login
            lastWindowTitle.clear();
            lastProcessName.clear();
            activityStartTime = QDateTime();

            // Reset keystroke buffer
            keystrokeBuffer.clear();


            // Test file writing immediately
            QString testFileName = QString("activity_log_%1.txt").arg(currentUser);
            QFile testFile(testFileName);
            if (testFile.open(QIODevice::Append | QIODevice::Text)) {
                QTextStream out(&testFile);
                out << "=== Logging started at " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << " ===\n";
                testFile.close();
            }

            activityTimer->start();

            // Install keyboard hook
            keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboardHookCallback, GetModuleHandle(NULL), 0);
            if (keyboardHook == nullptr) {
                QMessageBox::warning(this, "Warning", "Failed to install keyboard hook");
            }

            // Install mouse hook
            mouseHook = SetWindowsHookEx(
                WH_MOUSE_LL,
                mouseHookCallback,
                GetModuleHandle(NULL),
                0
            );
            mouseDistance = 0;hasLastMousePoint = false;

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

        QDateTime now = QDateTime::currentDateTime();

        // First activity
        if(lastWindowTitle.isEmpty())
        {
            lastWindowTitle = title;
            lastProcessName = processName;
            activityStartTime = now;
            return;
        }

        // Same activity -> continue timing
        if(lastWindowTitle == title &&
        lastProcessName == processName)
        {
            return;
        }

        // Activity changed

        QString logFileName = QString("activity_log_%1.txt").arg(currentUser);

        QFile logFile(logFileName);

        if(logFile.open(QIODevice::Append | QIODevice::Text))
        {
            QTextStream out(&logFile);

            // Get keystroke buffer (thread-safe)
            keystrokeMutex.lock();
            QString keystrokes = keystrokeBuffer;
            keystrokeBuffer.clear();
            keystrokeMutex.unlock();

            // Get mouse movement (thread-safe)
            mouseMutex.lock();
            double pixels = mouseDistance;
            mouseDistance = 0;
            mouseMutex.unlock();

            out
                << activityStartTime.toString("yyyy-MM-dd hh:mm:ss")
                << " - "
                << now.toString("yyyy-MM-dd hh:mm:ss")
                << " | "
                << lastWindowTitle
                << " | "
                << lastProcessName
                << " | "
                << currentUser
                << " | \""
                << keystrokes
                << "\" | Mouse Distance: "
                << pixels
                << " px\n";

            logFile.close();
        }

        // Start new activity
        lastWindowTitle = title;
        lastProcessName = processName;
        activityStartTime = now;




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

    // Flush the last activity on logout
    if(!lastWindowTitle.isEmpty())
    {
            QFile logFile(QString("activity_log_%1.txt").arg(currentUser));

            if(logFile.open(QIODevice::Append | QIODevice::Text))
            {
                QTextStream out(&logFile);

                // Get keystroke buffer (thread-safe)
                keystrokeMutex.lock();
                QString keystrokes = keystrokeBuffer;
                keystrokeBuffer.clear();
                keystrokeMutex.unlock();

                out
                    << activityStartTime.toString("yyyy-MM-dd hh:mm:ss")
                    << " - "
                    << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                    << " | "
                    << lastWindowTitle
                    << " | "
                    << lastProcessName
                    << " | "
                    << currentUser
                    << " | \""
                    << keystrokes
                    << "\""
                    << "\n";

                logFile.close();
            }
    }

    // Remove keyboard hook
    if (keyboardHook != nullptr) {
        UnhookWindowsHookEx(keyboardHook);
        keyboardHook = nullptr;
    }
    
    // Remove mouse hook
    if(mouseHook != nullptr)
{
    UnhookWindowsHookEx(mouseHook);
    mouseHook = nullptr;
}

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


        if(!lastWindowTitle.isEmpty())
        {
            QFile logFile(QString("activity_log_%1.txt").arg(currentUser));

            if(logFile.open(QIODevice::Append | QIODevice::Text))
            {
                QTextStream out(&logFile);

                // Get keystroke buffer (thread-safe)
                keystrokeMutex.lock();
                QString keystrokes = keystrokeBuffer;
                keystrokeBuffer.clear();
                keystrokeMutex.unlock();

                out
                    << activityStartTime.toString("yyyy-MM-dd hh:mm:ss")
                    << " - "
                    << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                    << " | "
                    << lastWindowTitle
                    << " | "
                    << lastProcessName
                    << " | "
                    << currentUser
                    << " | \""
                    << keystrokes
                    << "\""
                    << "\n";

                logFile.close();
            }
        }

        // Remove keyboard hook
        if (keyboardHook != nullptr) {
            UnhookWindowsHookEx(keyboardHook);
            keyboardHook = nullptr;
        }

        // Remove mouse hook
        if(mouseHook != nullptr)
        {
            UnhookWindowsHookEx(mouseHook);
            mouseHook = nullptr;
        }

        activityTimer->stop();
        isLogging = false;
    }
    QMainWindow::closeEvent(event);
}

// Keyboard hook callback function
LRESULT CALLBACK MainWindow::keyboardHookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && instance != nullptr) {
        KBDLLHOOKSTRUCT* kbStruct = (KBDLLHOOKSTRUCT*)lParam;
        int vkCode = kbStruct->vkCode;
        bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);

        if (isKeyDown) {
            // Get foreground window to check if it's our own application
            HWND hwnd = GetForegroundWindow();
            if (hwnd != NULL) {
                DWORD processId;
                GetWindowThreadProcessId(hwnd, &processId);
                DWORD currentProcessId = GetCurrentProcessId();

                // Filter out keystrokes from our own application
                if (processId != currentProcessId) {
                    QString keyName = instance->getKeyName(vkCode, isKeyDown);
                    if (!keyName.isEmpty()) {
                        // Thread-safe buffer update
                        instance->keystrokeMutex.lock();
                        instance->keystrokeBuffer += keyName;
                        instance->keystrokeMutex.unlock();
                    }
                }
            }
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// Get key name from virtual key code
QString MainWindow::getKeyName(int vkCode, bool isKeyDown)
{
    // Special keys
    switch (vkCode) {
        case VK_RETURN: return "[Enter]";
        case VK_BACK: return "[Backspace]";
        case VK_TAB: return "[Tab]";
        case VK_ESCAPE: return "[Escape]";
        case VK_SPACE: return " ";
        case VK_SHIFT: return "[Shift]";
        case VK_CONTROL: return "[Ctrl]";
        case VK_MENU: return "[Alt]";
        case VK_CAPITAL: return "[CapsLock]";
        case VK_LWIN: return "[Win]";
        case VK_RWIN: return "[Win]";
        case VK_APPS: return "[Menu]";
        case VK_INSERT: return "[Insert]";
        case VK_DELETE: return "[Delete]";
        case VK_HOME: return "[Home]";
        case VK_END: return "[End]";
        case VK_PRIOR: return "[PageUp]";
        case VK_NEXT: return "[PageDown]";
        case VK_LEFT: return "[Left]";
        case VK_RIGHT: return "[Right]";
        case VK_UP: return "[Up]";
        case VK_DOWN: return "[Down]";
        case VK_F1: return "[F1]";
        case VK_F2: return "[F2]";
        case VK_F3: return "[F3]";
        case VK_F4: return "[F4]";
        case VK_F5: return "[F5]";
        case VK_F6: return "[F6]";
        case VK_F7: return "[F7]";
        case VK_F8: return "[F8]";
        case VK_F9: return "[F9]";
        case VK_F10: return "[F10]";
        case VK_F11: return "[F11]";
        case VK_F12: return "[F12]";
    }

    // Check for modifier combinations
    BYTE keyboardState[256];
    GetKeyboardState(keyboardState);

    bool shiftPressed = (keyboardState[VK_SHIFT] & 0x80) != 0;
    bool ctrlPressed = (keyboardState[VK_CONTROL] & 0x80) != 0;
    bool altPressed = (keyboardState[VK_MENU] & 0x80) != 0;

    // Convert virtual key to character
    wchar_t buffer[10];
    int result = ToUnicode(vkCode, MapVirtualKey(vkCode, 0), keyboardState, buffer, 10, 0);

    if (result > 0) {
        QString key = QString::fromWCharArray(buffer, result);

        // Handle modifier combinations
        if (ctrlPressed && altPressed) {
            return QString("[Ctrl+Alt+%1]").arg(key.toUpper());
        } else if (ctrlPressed) {
            return QString("[Ctrl+%1]").arg(key.toUpper());
        } else if (altPressed) {
            return QString("[Alt+%1]").arg(key.toUpper());
        } else if (shiftPressed) {
            return key.toUpper();
        } else {
            return key.toLower();
        }
    }

    return "";
}

// Mouse hook callback function
LRESULT CALLBACK MainWindow::mouseHookCallback(int nCode,
                                               WPARAM wParam,
                                               LPARAM lParam)
{
    if (nCode >= 0 &&
        wParam == WM_MOUSEMOVE &&
        instance != nullptr)
    {
        MSLLHOOKSTRUCT *mouse =
                reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);

        instance->mouseMutex.lock();

        if(!instance->hasLastMousePoint)
        {
            instance->lastMousePoint = mouse->pt;
            instance->hasLastMousePoint = true;
        }
        else
        {
            LONG dx = mouse->pt.x - instance->lastMousePoint.x;
            LONG dy = mouse->pt.y - instance->lastMousePoint.y;

            instance->mouseDistance += std::hypot(
                        static_cast<double>(dx),
                        static_cast<double>(dy));

            instance->lastMousePoint = mouse->pt;
        }

        instance->mouseMutex.unlock();
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}