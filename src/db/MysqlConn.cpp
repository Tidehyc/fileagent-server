#include "MysqlConn.h"

namespace fileagent
{

    MysqlConn::MysqlConn()
    {
        connection_ = mysql_init(nullptr);
        if (connection_ != nullptr)
        {
            mysql_set_character_set(connection_, "utf8mb4");
        }
        refreshAliveTime();
    }

    MysqlConn::~MysqlConn()
    {
        freeResult();
        if (connection_ != nullptr)
        {
            mysql_close(connection_);
            connection_ = nullptr;
        }
    }

    bool MysqlConn::connect(const std::string &user,
                            const std::string &password,
                            const std::string &db_name,
                            const std::string &host,
                            unsigned short port)
    {
        if (connection_ == nullptr)
        {
            return false;
        }

        MYSQL *result = mysql_real_connect(connection_, host.c_str(), user.c_str(), password.c_str(), db_name.c_str(), port, nullptr, 0);
        if (result == nullptr)
        {
            return false;
        }

        mysql_set_character_set(connection_, "utf8mb4");
        refreshAliveTime();
        return true;
    }

    bool MysqlConn::update(const std::string &sql)
    {
        return mysql_query(connection_, sql.c_str()) == 0;
    }

    bool MysqlConn::query(const std::string &sql)
    {
        freeResult();
        if (mysql_query(connection_, sql.c_str()) != 0)
        {
            return false;
        }

        result_ = mysql_store_result(connection_);
        return result_ != nullptr;
    }

    bool MysqlConn::next()
    {
        if (result_ == nullptr)
        {
            return false;
        }

        row_ = mysql_fetch_row(result_);
        return row_ != nullptr;
    }

    const char *MysqlConn::value(int index) const
    {
        if (result_ == nullptr || row_ == nullptr)
        {
            return nullptr;
        }

        int field_count = mysql_num_fields(result_);
        if (index < 0 || index >= field_count)
        {
            return nullptr;
        }

        return row_[index];
    }

    bool MysqlConn::transaction()
    {
        return mysql_autocommit(connection_, 0) == 0;
    }

    bool MysqlConn::commit()
    {
        if (mysql_commit(connection_) != 0)
        {
            return false;
        }

        return mysql_autocommit(connection_, 1) == 0;
    }

    bool MysqlConn::rollback()
    {
        if (mysql_rollback(connection_) != 0)
        {
            return false;
        }

        return mysql_autocommit(connection_, 1) == 0;
    }

    std::string MysqlConn::escape(const std::string &text) const
    {
        if (connection_ == nullptr || text.empty())
        {
            return text;
        }

        std::string escaped;
        escaped.resize(text.size() * 2 + 1);
        unsigned long length = mysql_real_escape_string(connection_, escaped.data(), text.c_str(), static_cast<unsigned long>(text.size()));
        escaped.resize(length);
        return escaped;
    }

    void MysqlConn::freeResult()
    {
        if (result_ != nullptr)
        {
            mysql_free_result(result_);
            result_ = nullptr;
            row_ = nullptr;
        }
    }

    void MysqlConn::refreshAliveTime()
    {
        alive_time_ = std::chrono::steady_clock::now();
    }

    long long MysqlConn::getAliveTime() const
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - alive_time_).count();
    }

} // namespace fileagent