#include "FileDao.h"

#include "../db/ConnectionPool.h"
#include "../db/MysqlConn.h"

namespace fileagent
{

    namespace
    {

        FileRecord makeFileRecord(const ConnectionGuard &connection)
        {
            FileRecord record;
            record.id = connection->value(0) != nullptr ? std::stoll(connection->value(0)) : 0;
            record.user_id = connection->value(1) != nullptr ? std::stoll(connection->value(1)) : 0;
            record.filename = connection->value(2) != nullptr ? connection->value(2) : "";
            record.file_hash = connection->value(3) != nullptr ? connection->value(3) : "";
            record.file_size = connection->value(4) != nullptr ? std::stoll(connection->value(4)) : 0;
            record.storage_path = connection->value(5) != nullptr ? connection->value(5) : "";
            record.status = connection->value(6) != nullptr ? std::stoi(connection->value(6)) : 0;
            record.created_at = connection->value(7) != nullptr ? connection->value(7) : "";
            return record;
        }

    } // namespace

    bool FileDao::createFile(const FileRecord &record)
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection)
        {
            return false;
        }

        std::string sql = "INSERT INTO files (user_id, filename, file_hash, file_size, storage_path, status) VALUES (" +
                          std::to_string(record.user_id) + ", '" +
                          connection->escape(record.filename) + "', '" +
                          connection->escape(record.file_hash) + "', " +
                          std::to_string(record.file_size) + ", '" +
                          connection->escape(record.storage_path) + "', " +
                          std::to_string(record.status) + ")";
        return connection->update(sql);
    }

    std::optional<FileRecord> FileDao::findById(std::int64_t id)
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection)
        {
            return std::nullopt;
        }

        std::string sql = "SELECT id, user_id, filename, file_hash, file_size, storage_path, status, created_at FROM files WHERE id = " +
                          std::to_string(id) + " LIMIT 1";
        if (!connection->query(sql) || !connection->next())
        {
            return std::nullopt;
        }

        return makeFileRecord(connection);
    }

    std::optional<FileRecord> FileDao::findByHash(const std::string &file_hash)
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection)
        {
            return std::nullopt;
        }

        std::string sql = "SELECT id, user_id, filename, file_hash, file_size, storage_path, status, created_at FROM files WHERE file_hash = '" +
                          connection->escape(file_hash) + "' LIMIT 1";
        if (!connection->query(sql) || !connection->next())
        {
            return std::nullopt;
        }

        return makeFileRecord(connection);
    }

    std::vector<FileRecord> FileDao::listByUserId(std::int64_t user_id, int limit, int offset)
    {
        std::vector<FileRecord> files;
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection)
        {
            return files;
        }

        std::string sql = "SELECT id, user_id, filename, file_hash, file_size, storage_path, status, created_at FROM files WHERE user_id = " +
                          std::to_string(user_id) + " ORDER BY id DESC LIMIT " + std::to_string(limit) +
                          " OFFSET " + std::to_string(offset);
        if (!connection->query(sql))
        {
            return files;
        }

        while (connection->next())
        {
            files.push_back(makeFileRecord(connection));
        }
        return files;
    }

    bool FileDao::updateStatus(std::int64_t id, int status)
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection)
        {
            return false;
        }

        std::string sql = "UPDATE files SET status = " + std::to_string(status) + " WHERE id = " + std::to_string(id);
        return connection->update(sql);
    }

    bool FileDao::removeById(std::int64_t id)
    {
        auto connection = ConnectionPool::instance().getConnection();
        if (!connection)
        {
            return false;
        }

        std::string sql = "DELETE FROM files WHERE id = " + std::to_string(id);
        return connection->update(sql);
    }

} // namespace fileagent