#include "AuthService.h"

#include "../common/Hash.h"
#include "../common/Logger.h"
#include "../common/StringUtil.h"
#include "../dao/UserDao.h"

namespace fileagent
{

    bool AuthService::validateUsername(const std::string &username, std::string &message) const
    {
        if (username.empty())
        {
            message = "username is empty";
            return false;
        }

        if (username.size() < 3 || username.size() > 32)
        {
            message = "username length must be between 3 and 32";
            return false;
        }

        return true;
    }

    bool AuthService::validatePassword(const std::string &password, std::string &message) const
    {
        if (password.size() < 8)
        {
            message = "password length must be at least 8";
            return false;
        }

        return true;
    }

    bool AuthService::validateEmail(const std::string &email, std::string &message) const
    {
        if (email.empty())
        {
            message = "email is empty";
            return false;
        }

        auto at_pos = email.find('@');
        if (at_pos == std::string::npos || at_pos == 0 || at_pos + 1 >= email.size())
        {
            message = "email format is invalid";
            return false;
        }

        return true;
    }

    ServiceResult<UserRecord> AuthService::registerUser(const RegisterRequest &request)
    {
        RegisterRequest normalized{
            trimCopy(request.username),
            trimCopy(request.password),
            trimCopy(request.email),
        };

        std::string message;
        if (!validateUsername(normalized.username, message) ||
            !validatePassword(normalized.password, message) ||
            !validateEmail(normalized.email, message))
        {
            return ServiceResult<UserRecord>::fail(message);
        }

        UserDao user_dao;
        if (user_dao.findByUsername(normalized.username).has_value())
        {
            return ServiceResult<UserRecord>::fail("username already exists");
        }

        std::string hashed_password = PasswordHasher::hashPassword(normalized.password);
        if (hashed_password.empty())
        {
            return ServiceResult<UserRecord>::fail("failed to hash password");
        }

        if (!user_dao.createUser(normalized.username, hashed_password, normalized.email))
        {
            logError("failed to create user record");
            return ServiceResult<UserRecord>::fail("failed to create user");
        }

        auto created_user = user_dao.findByUsername(normalized.username);
        if (!created_user.has_value())
        {
            return ServiceResult<UserRecord>::fail("user created but cannot be reloaded");
        }

        return ServiceResult<UserRecord>::ok(*created_user, "register success");
    }

    ServiceResult<UserRecord> AuthService::login(const LoginRequest &request)
    {
        LoginRequest normalized{
            trimCopy(request.username),
            trimCopy(request.password),
        };

        std::string message;
        if (!validateUsername(normalized.username, message) ||
            !validatePassword(normalized.password, message))
        {
            return ServiceResult<UserRecord>::fail(message);
        }

        UserDao user_dao;
        auto user = user_dao.findByUsername(normalized.username);
        if (!user.has_value())
        {
            return ServiceResult<UserRecord>::fail("user not found");
        }

        if (user->status != 1)
        {
            return ServiceResult<UserRecord>::fail("user is disabled");
        }

        if (!PasswordHasher::verifyPassword(normalized.password, user->password))
        {
            return ServiceResult<UserRecord>::fail("password is incorrect");
        }

        return ServiceResult<UserRecord>::ok(*user, "login success");
    }

    ServiceStatus AuthService::updateStatus(std::int64_t user_id, int status)
    {
        if (user_id <= 0)
        {
            return ServiceStatus::fail("user id is invalid");
        }

        UserDao user_dao;
        if (!user_dao.updateStatus(user_id, status))
        {
            return ServiceStatus::fail("failed to update user status");
        }

        return ServiceStatus::ok("update status success");
    }

} // namespace fileagent