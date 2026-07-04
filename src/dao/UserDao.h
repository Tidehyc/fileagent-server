#pragma once

#include <optional>
#include <string>

#include "Models.h"

namespace fileagent
{

    class UserDao
    {
    public:
        bool createUser(const std::string &username,
                        const std::string &password,
                        const std::string &email);

        std::optional<UserRecord> findById(std::int64_t id);
        std::optional<UserRecord> findByUsername(const std::string &username);

        bool updateStatus(std::int64_t id, int status);
        bool updatePassword(std::int64_t id, const std::string &password);
    };

} // namespace fileagent