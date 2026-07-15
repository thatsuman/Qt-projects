#ifndef AUTHMANAGER_H
#define AUTHMANAGER_H

#include <QString>
#include <QStringList>

/**
 * @brief Holds the result of an authentication attempt.
 */
struct AuthResult {
    bool    success;   ///< true if credentials matched
    QString username;  ///< matched username on success, empty on failure
};

/**
 * @brief AuthManager validates user credentials.
 *
 * Credentials are stored here as static data. To switch to a database
 * or config-file backend, only this class needs to change.
 */
class AuthManager
{
public:
    AuthManager();

    /**
     * @brief Attempt to authenticate with the given credentials.
     * @param username  The username entered by the user.
     * @param password  The password entered by the user.
     * @return AuthResult with success flag and matched username.
     */
    AuthResult authenticate(const QString &username, const QString &password) const;

private:
    QStringList m_usernames;
    QStringList m_passwords;
};

#endif // AUTHMANAGER_H
