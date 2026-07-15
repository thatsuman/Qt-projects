#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QCloseEvent>
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
};
#endif // MAINWINDOW_H
