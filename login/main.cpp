#include "mainwindow.h"

#include <QApplication>
#include <QMessageBox>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {

bool isRunningAsAdministrator()
{
    BOOL isAdmin = FALSE;
    PSID administratorsGroup = nullptr;

    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (!AllocateAndInitializeSid(&ntAuthority,
                                  2,
                                  SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS,
                                  0, 0, 0, 0, 0, 0,
                                  &administratorsGroup)) {
        return false;
    }

    if (!CheckTokenMembership(nullptr, administratorsGroup, &isAdmin)) {
        isAdmin = FALSE;
    }

    FreeSid(administratorsGroup);
    return isAdmin == TRUE;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    const bool forceUnelevatedForTest = qEnvironmentVariableIsSet("LOGIN_FORCE_UNELEVATED");
    if (forceUnelevatedForTest || !isRunningAsAdministrator()) {
        if (!qEnvironmentVariableIsSet("LOGIN_SUPPRESS_ADMIN_DIALOG")) {
            QMessageBox::critical(nullptr,
                                  "Administrator Privileges Required",
                                  "This application requires administrator privileges for network logging. "
                                  "Please restart it and accept the Windows UAC prompt.");
        }
        return 1;
    }

    MainWindow w;

    w.setWindowTitle("Login Window");

    w.show();
    return QApplication::exec();
}
