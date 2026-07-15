#ifndef LOGINUIMANAGER_H
#define LOGINUIMANAGER_H

// Forward declaration — do not include ui_mainwindow.h here
namespace Ui { class MainWindow; }

/**
 * @brief LoginUIManager manages the visibility transitions of login-form
 *        and logged-in-state widgets so that MainWindow does not need to
 *        enumerate individual widget pointers every time.
 *
 * Usage:
 *   LoginUIManager uiMgr(ui);
 *   uiMgr.showLoggedInState();  // after successful login
 *   uiMgr.showLoginForm();      // after logout
 *   uiMgr.togglePasswordVisibility();
 */
class LoginUIManager
{
public:
    explicit LoginUIManager(Ui::MainWindow *ui);

    /** @brief Hide the login form and show the logged-in controls (e.g., Logout button). */
    void showLoggedInState();

    /** @brief Show the login form and hide logged-in controls. Also clears input fields. */
    void showLoginForm();

    /** @brief Toggle password field echo mode between Password and Normal. */
    void togglePasswordVisibility();

private:
    Ui::MainWindow *m_ui;
};

#endif // LOGINUIMANAGER_H
