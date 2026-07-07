#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <mysql/mysql.h>

namespace fileagent
{

class MysqlConn;

/**
 * @brief MySQL Prepared Statement RAII 封装
 *
 * 使用 mysql_stmt_* API，替代字符串拼接的 escape() 方式。
 * 自动管理 stmt 生命周期。
 *
 * 用法示例：
 *   PreparedStatement stmt(conn);
 *   stmt.prepare("SELECT id FROM users WHERE username = ?");
 *   stmt.bindString(0, username);
 *   stmt.execute();
 *   while (stmt.fetch()) { ... }
 */
class PreparedStatement
{
public:
    /// 绑定参数类型
    enum class Type
    {
        STRING,
        INT64,
        DOUBLE,
        BLOB,
    };

    explicit PreparedStatement(MYSQL *mysql);
    ~PreparedStatement();

    PreparedStatement(const PreparedStatement &) = delete;
    PreparedStatement &operator=(const PreparedStatement &) = delete;

    /// 移动构造
    PreparedStatement(PreparedStatement &&other) noexcept;
    PreparedStatement &operator=(PreparedStatement &&other) noexcept;

    /**
     * @brief 准备 SQL 语句（使用 ? 占位符）
     */
    bool prepare(const std::string &sql);

    /**
     * @brief 绑定字符串参数
     * @param index 参数索引（从 0 开始）
     */
    bool bindString(int index, const std::string &value);

    /**
     * @brief 绑定整数参数
     * @param index 参数索引（从 0 开始）
     */
    bool bindInt64(int index, std::int64_t value);

    /**
     * @brief 绑定二进制参数
     */
    bool bindBlob(int index, const std::string &value);

    /**
     * @brief 执行语句
     */
    bool execute();

    /**
     * @brief 获取下一行
     */
    bool fetch();

    /**
     * @brief 获取结果集中的列值（字符串）
     */
    std::string getString(int index) const;

    /**
     * @brief 获取结果集中的列值（整数）
     */
    std::int64_t getInt64(int index) const;

    /**
     * @brief 获取影响行数
     */
    int affectedRows() const;

    /**
     * @brief 获取结果列数
     */
    int fieldCount() const;

    /// 是否已准备好
    bool isPrepared() const { return stmt_ != nullptr; }

private:
    void freeStmt();
    bool bindParams();

    MYSQL *mysql_;
    MYSQL_STMT *stmt_{nullptr};
    MYSQL_RES *metadata_{nullptr};
    MYSQL_BIND *params_{nullptr};
    MYSQL_BIND *results_{nullptr};
    int param_count_{0};
    int result_count_{0};

    // 参数数据存储（确保绑定数据生命周期）
    std::vector<std::string> param_strings_;
    std::vector<unsigned long> param_lengths_;
};

} // namespace fileagent
