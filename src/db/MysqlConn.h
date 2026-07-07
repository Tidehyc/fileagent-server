#pragma once

#include <chrono>
#include <mysql/mysql.h>
#include <string>

namespace fileagent
{

    class MysqlConn
    {
    public:
        MysqlConn();
        ~MysqlConn();

        MysqlConn(const MysqlConn &) = delete;
        MysqlConn &operator=(const MysqlConn &) = delete;

        bool connect(const std::string &user,
                     const std::string &password,
                     const std::string &db_name,
                     const std::string &host,
                     unsigned short port = 3306);

        bool update(const std::string &sql);
        bool query(const std::string &sql);
        bool next();
        const char *value(int index) const;

        bool transaction();
        bool commit();
        bool rollback();

        std::string escape(const std::string &text) const;
        MYSQL *raw() { return connection_; }

        void refreshAliveTime();
        long long getAliveTime() const;

    private:
        void freeResult();

        MYSQL *connection_{nullptr};
        MYSQL_RES *result_{nullptr};
        MYSQL_ROW row_{nullptr};
        std::chrono::steady_clock::time_point alive_time_;
    };

} // namespace fileagent