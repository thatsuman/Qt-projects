#ifndef UI_DASHBOARDCONTROLLER_H
#define UI_DASHBOARDCONTROLLER_H

#include <QObject>
#include <QString>

// DashboardController orchestrates the full analytics pipeline:
// reads logs → aggregates → generates HTML → opens in default browser.
// MainWindow calls only generateAndOpenDashboard() — all analytics logic stays here.
class DashboardController : public QObject
{
    Q_OBJECT

public:
    explicit DashboardController(QObject *parent = nullptr);

    // Generates dashboard.html on disk for the given username without opening browser.
    // Returns the file path on success, or empty string on failure.
    QString generateDashboard(const QString &username);

    // Runs full pipeline: generates dashboard.html AND opens default browser.
    void generateAndOpenDashboard(const QString &username);

signals:
    void statusMessage(const QString &message);
};

#endif // UI_DASHBOARDCONTROLLER_H
