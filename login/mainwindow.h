#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QCloseEvent>
#include <QMutex>
#include <windows.h>
#include <psapi.h>

QT_BEGIN_NAMESPACE

namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void okButton();
    void togglePasswordVisibility();
    void logActivity();
    void logoutButton();

private:
    Ui::MainWindow *ui;
    QTimer *activityTimer;
    QString currentUser;
    bool isLogging;

    QString lastWindowTitle;
    QString lastProcessName;

    QDateTime activityStartTime;

    // Keyboard hook members
    HHOOK keyboardHook;
    QString keystrokeBuffer;
    QMutex keystrokeMutex;
    static MainWindow* instance; // For static hook callback

    static LRESULT CALLBACK keyboardHookCallback(int nCode, WPARAM wParam, LPARAM lParam);

    // Mouse hook members
    HHOOK mouseHook;
    POINT lastMousePoint;
    bool hasLastMousePoint;
    double mouseDistance;
    QMutex mouseMutex;

    static LRESULT CALLBACK mouseHookCallback(int nCode, WPARAM wParam, LPARAM lParam);


    QString getKeyName(int vkCode, bool isKeyDown);
};
#endif // MAINWINDOW_H
