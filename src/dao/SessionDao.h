#pragma once

#include <optional>
#include <string>

#include "Models.h"

namespace fileagent
{

    class SessionDao
    {
    public:
        /**
         * @brief 创建新 session
         * @param user_id  用户ID
         * @param token    session token 字符串
         * @param expiresAt 过期时间（格式: YYYY-MM-DD HH:MM:SS）
         * @return true 创建成功
         */
        bool createSession(std::int64_t user_id, const std::string &token, const std::string &expiresAt);

        /**
         * @brief 按 token 查找有效 session（自动检查过期）
         */
        std::optional<SessionRecord> findByToken(const std::string &token);

        /**
         * @brief 按 token 删除 session（登出）
         */
        bool deleteByToken(const std::string &token);

        /**
         * @brief 删除用户所有 session（强制下线）
         */
        bool deleteByUserId(std::int64_t user_id);

        /**
         * @brief 清理所有过期 session
         * @return 删除的记录数
         */
        int deleteExpiredSessions();
    };

} // namespace fileagent
