#include "ShareDao.h"

#include "../db/ConnectionPool.h"
#include "../db/MysqlConn.h"
#include "../common/Logger.h"

namespace fileagent
{

    namespace
    {

        ShareRecord makeShareRecord(const ConnectionGuard &connection)
        {
            ShareRecord record;
            record.id = connection->value(0) ? std::stoll(connection->value(0)) : 0;
            record.file_id = connection->value(1) ? std::stoll(connection->value(1)) : 0;
            record.user_id = connection->value(2) ? std::stoll(connection->value(2)) : 0;
            record.share_token = connection->value(3) ? connection->value(3) : "";
            record.expires_at = connection->value(4) ? connection->value(4) : "";
            record.created_at = connection->value(5) ? connection->value(5) : "";
            return record;
        }

    } // anonymous namespace

    bool ShareDao::createShare(std::int64_t file_id,
                                std::int64_t user_id,
                                const std::string &token,
                                const std::string &expiresAt)
    {
        auto conn = ConnectionPool::instance().getConnection();
        if (!conn)
        {
            logError("ShareDao::createShare: no connection");
            return false;
        }

        std::string sql;
        if (expiresAt.empty())
        {
            sql = "INSERT INTO shares (file_id, user_id, share_token) VALUES (" +
                  std::to_string(file_id) + ", " +
                  std::to_string(user_id) + ", '" +
                  conn->escape(token) + "')";
        }
        else
        {
            sql = "INSERT INTO shares (file_id, user_id, share_token, expires_at) VALUES (" +
                  std::to_string(file_id) + ", " +
                  std::to_string(user_id) + ", '" +
                  conn->escape(token) + "', '" +
                  conn->escape(expiresAt) + "')";
        }
        return conn->update(sql);
    }

    std::optional<ShareRecord> ShareDao::findByToken(const std::string &token)
    {
        auto conn = ConnectionPool::instance().getConnection();
        if (!conn)
        {
            logError("ShareDao::findByToken: no connection");
            return std::nullopt;
        }

        std::string sql = "SELECT id, file_id, user_id, share_token, expires_at, created_at "
                          "FROM shares WHERE share_token = '" +
                          conn->escape(token) +
                          "' AND (expires_at IS NULL OR expires_at > NOW()) LIMIT 1";
        if (!conn->query(sql) || !conn->next())
        {
            return std::nullopt;
        }
        return makeShareRecord(conn);
    }

    bool ShareDao::deleteByToken(const std::string &token)
    {
        auto conn = ConnectionPool::instance().getConnection();
        if (!conn)
        {
            logError("ShareDao::deleteByToken: no connection");
            return false;
        }

        std::string sql = "DELETE FROM shares WHERE share_token = '" +
                          conn->escape(token) + "'";
        return conn->update(sql);
    }

    std::vector<ShareRecord> ShareDao::listByUserId(std::int64_t user_id)
    {
        std::vector<ShareRecord> records;
        auto conn = ConnectionPool::instance().getConnection();
        if (!conn)
        {
            logError("ShareDao::listByUserId: no connection");
            return records;
        }

        std::string sql = "SELECT id, file_id, user_id, share_token, expires_at, created_at "
                          "FROM shares WHERE user_id = " +
                          std::to_string(user_id) + " ORDER BY id DESC";
        if (!conn->query(sql))
        {
            return records;
        }

        while (conn->next())
        {
            records.push_back(makeShareRecord(conn));
        }
        return records;
    }

} // namespace fileagent
