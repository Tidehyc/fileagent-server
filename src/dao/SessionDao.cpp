#include "SessionDao.h"

#include <cstdint>

#include "../common/CacheManager.h"
#include "../db/ConnectionPool.h"
#include "../db/MysqlConn.h"
#include "../common/Logger.h"

namespace fileagent
{

    namespace
    {

        SessionRecord makeSessionRecord(const ConnectionGuard &connection)
        {
            SessionRecord record;
            record.id = connection->value(0) != nullptr ? std::stoll(connection->value(0)) : 0;
            record.user_id = connection->value(1) != nullptr ? std::stoll(connection->value(1)) : 0;
            record.session_token = connection->value(2) != nullptr ? connection->value(2) : "";
            record.expires_at = connection->value(3) != nullptr ? connection->value(3) : "";
            record.created_at = connection->value(4) != nullptr ? connection->value(4) : "";
            return record;
        }

    } // anonymous namespace

    bool SessionDao::createSession(std::int64_t user_id, const std::string &token, const std::string &expiresAt)
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection)
        {
            logError("SessionDao::createSession: no connection available");
            return false;
        }

        std::string sql = "INSERT INTO sessions (user_id, session_token, expires_at) VALUES (" +
                          std::to_string(user_id) + ", '" +
                          connection->escape(token) + "', '" +
                          connection->escape(expiresAt) + "')";
        return connection->update(sql);
    }

    std::optional<SessionRecord> SessionDao::findByToken(const std::string &token)
    {
        // 查缓存
        auto &cache = CacheManager::instance().sessionCache();
        auto cached = cache.get(token);
        if (cached.has_value())
        {
            return cached;
        }

        // 未命中，查库
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection)
        {
            logError("SessionDao::findByToken: no connection available");
            return std::nullopt;
        }

        std::string sql = "SELECT id, user_id, session_token, expires_at, created_at "
                          "FROM sessions WHERE session_token = '" +
                          connection->escape(token) + "' AND expires_at > NOW() LIMIT 1";
        if (!connection->query(sql) || !connection->next())
        {
            return std::nullopt;
        }

        auto session = makeSessionRecord(connection);

        // 写入缓存
        cache.put(session.session_token, session);

        return session;
    }

    bool SessionDao::deleteByToken(const std::string &token)
    {
        // 从缓存删除
        CacheManager::instance().sessionCache().remove(token);

        auto connection = ConnectionPool::instance().getConnection();
        if (!connection)
        {
            logError("SessionDao::deleteByToken: no connection available");
            return false;
        }

        std::string sql = "DELETE FROM sessions WHERE session_token = '" +
                          connection->escape(token) + "'";
        return connection->update(sql);
    }

    bool SessionDao::deleteByUserId(std::int64_t user_id)
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection)
        {
            logError("SessionDao::deleteByUserId: no connection available");
            return false;
        }

        std::string sql = "DELETE FROM sessions WHERE user_id = " + std::to_string(user_id);
        return connection->update(sql);
    }

    int SessionDao::deleteExpiredSessions()
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection)
        {
            logError("SessionDao::deleteExpiredSessions: no connection available");
            return 0;
        }

        std::string sql = "DELETE FROM sessions WHERE expires_at <= NOW()";
        if (!connection->update(sql))
        {
            return 0;
        }

        // MySQL 的 affected_rows 可以通过 mysql_affected_rows 获取
        // 当前封装没有暴露该API，先返回 0 表示未知
        return 0;
    }

} // namespace fileagent
