#include "authmanager.h"

AuthManager::AuthManager()
{
    // Credentials list — to swap for a DB/config file, only change this constructor.
    m_usernames = {"jack", "john", "recon"};
    m_passwords = {"jack123", "john123", "recon123"};
}

AuthResult AuthManager::authenticate(const QString &username, const QString &password) const
{
    for (int i = 0; i < m_usernames.size(); ++i) {
        if (username == m_usernames[i] && password == m_passwords[i]) {
            return AuthResult{true, m_usernames[i]};
        }
    }
    return AuthResult{false, QString()};
}
