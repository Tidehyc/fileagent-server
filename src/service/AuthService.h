#pragma once

#include <string>

#include "../dao/Models.h"
#include "ServiceTypes.h"

namespace fileagent
{

    struct RegisterRequest
    {
        std::string username;
        std::string password;
        std::string email;
    };

    struct LoginRequest
    {
        std::string username;
        std::string password;
    };

    class AuthService
    {
    public:
        ServiceResult<UserRecord> registerUser(const RegisterRequest &request);
        ServiceResult<UserRecord> login(const LoginRequest &request);
        ServiceStatus updateStatus(std::int64_t user_id, int status);

    private:
        bool validateUsername(const std::string &username, std::string &message) const;
        bool validatePassword(const std::string &password, std::string &message) const;
        bool validateEmail(const std::string &email, std::string &message) const;
    };

} // namespace fileagent