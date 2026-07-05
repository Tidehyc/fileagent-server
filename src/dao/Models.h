#pragma once

#include <cstdint>
#include <string>

namespace fileagent
{

    struct UserRecord
    {
        std::int64_t id{0};
        std::string username;
        std::string password;
        std::string email;
        int status{0};
        bool is_admin{false};
        std::string created_at;
        std::string updated_at;
    };

    struct SessionRecord
    {
        std::int64_t id{0};
        std::int64_t user_id{0};
        std::string session_token;
        std::string expires_at;
        std::string created_at;
    };

    struct ShareRecord
    {
        std::int64_t id{0};
        std::int64_t file_id{0};
        std::int64_t user_id{0};
        std::string share_token;
        std::string expires_at;
        std::string created_at;
    };

    struct FileRecord
    {
        std::int64_t id{0};
        std::int64_t user_id{0};
        std::string filename;
        std::string file_hash;
        std::int64_t file_size{0};
        std::string storage_path;
        int status{0};
        std::string created_at;
    };

} // namespace fileagent