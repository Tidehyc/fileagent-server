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
            record.is_admin = connection->value(5) != nullptr && std::string(connection->value(5)) == "1";
            record.created_at = connection->value(6) != nullptr ? connection->value(6) : "";
            record.updated_at = connection->value(7) != nullptr ? connection->value(7) : "";
            return record;
        }

    } // namespace

    bool UserDao::createUser(const std::string &username,
                             const std::string &password,
                             const std::string &email)
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection) return false;

        std::string sql = "INSERT INTO users (username, password, email, status) VALUES ('" +
                          connection->escape(username) + "', '" +
                          connection->escape(password) + "', '" +
                          connection->escape(email) + "', 1)";
        return connection->update(sql);
    }

    std::optional<UserRecord> UserDao::findById(std::int64_t id)
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection) return std::nullopt;

        std::string sql = "SELECT id, username, password, email, status, is_admin, created_at, updated_at FROM users WHERE id = " +
                          std::to_string(id) + " LIMIT 1";
        if (!connection->query(sql) || !connection->next())
            return std::nullopt;

        return makeUserRecord(connection);
    }

    std::optional<UserRecord> UserDao::findByUsername(const std::string &username)
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection) return std::nullopt;

        std::string sql = "SELECT id, username, password, email, status, is_admin, created_at, updated_at FROM users WHERE username = '" +
                          connection->escape(username) + "' LIMIT 1";
        if (!connection->query(sql) || !connection->next())
            return std::nullopt;

        return makeUserRecord(connection);
    }

    bool UserDao::updateStatus(std::int64_t id, int status)
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection) return false;

        std::string sql = "UPDATE users SET status = " + std::to_string(status) + ", updated_at = NOW() WHERE id = " + std::to_string(id);
        return connection->update(sql);
    }

    bool UserDao::updatePassword(std::int64_t id, const std::string &password)
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection) return false;

        std::string sql = "UPDATE users SET password = '" + connection->escape(password) + "', updated_at = NOW() WHERE id = " + std::to_string(id);
        return connection->update(sql);
    }

    std::vector<UserRecord> UserDao::listAll(int limit, int offset)
    {
        std::vector<UserRecord> users;
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection) return users;

        std::string sql = "SELECT id, username, password, email, status, is_admin, created_at, updated_at "
                          "FROM users ORDER BY id ASC LIMIT " + std::to_string(limit) +
                          " OFFSET " + std::to_string(offset);
        if (!connection->query(sql)) return users;

        while (connection->next())
            users.push_back(makeUserRecord(connection));

        return users;
    }

    std::int64_t UserDao::countAll()
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection) return 0;

        if (!connection->query("SELECT COUNT(*) FROM users") || !connection->next())
            return 0;

        return connection->value(0) ? std::stoll(connection->value(0)) : 0;
    }

} // namespace fileagent
