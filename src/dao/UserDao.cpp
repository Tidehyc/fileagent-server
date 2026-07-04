#include "UserDao.h"

#include "../db/ConnectionPool.h"
#include "../db/MysqlConn.h"

namespace fileagent
{

    namespace
    {

        UserRecord makeUserRecord(const ConnectionGuard &connection)
        {
            UserRecord record;
            record.id = connection->value(0) != nullptr ? std::stoll(connection->value(0)) : 0;
            record.username = connection->value(1) != nullptr ? connection->value(1) : "";
            record.password = connection->value(2) != nullptr ? connection->value(2) : "";
            record.email = connection->value(3) != nullptr ? connection->value(3) : "";
            record.status = connection->value(4) != nullptr ? std::stoi(connection->value(4)) : 0;
            record.created_at = connection->value(5) != nullptr ? connection->value(5) : "";
            record.updated_at = connection->value(6) != nullptr ? connection->value(6) : "";
            return record;
        }

    } // namespace

    bool UserDao::createUser(const std::string &username,
                             const std::string &password,
                             const std::string &email)
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection)
        {
            return false;
        }

        std::string sql = "INSERT INTO users (username, password, email, status) VALUES ('" +
                          connection->escape(username) + "', '" +
                          connection->escape(password) + "', '" +
                          connection->escape(email) + "', 1)";
        return connection->update(sql);
    }

    std::optional<UserRecord> UserDao::findById(std::int64_t id)
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection)
        {
            return std::nullopt;
        }

        std::string sql = "SELECT id, username, password, email, status, created_at, updated_at FROM users WHERE id = " +
                          std::to_string(id) + " LIMIT 1";
        if (!connection->query(sql) || !connection->next())
        {
            return std::nullopt;
        }

        return makeUserRecord(connection);
    }

    std::optional<UserRecord> UserDao::findByUsername(const std::string &username)
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection)
        {
            return std::nullopt;
        }

        std::string sql = "SELECT id, username, password, email, status, created_at, updated_at FROM users WHERE username = '" +
                          connection->escape(username) + "' LIMIT 1";
        if (!connection->query(sql) || !connection->next())
        {
            return std::nullopt;
        }

        return makeUserRecord(connection);
    }

    bool UserDao::updateStatus(std::int64_t id, int status)
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection)
        {
            return false;
        }

        std::string sql = "UPDATE users SET status = " + std::to_string(status) + ", updated_at = NOW() WHERE id = " + std::to_string(id);
        return connection->update(sql);
    }

    bool UserDao::updatePassword(std::int64_t id, const std::string &password)
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection)
        {
            return false;
        }

        std::string sql = "UPDATE users SET password = '" + connection->escape(password) + "', updated_at = NOW() WHERE id = " + std::to_string(id);
        return connection->update(sql);
    }

} // namespace fileagent