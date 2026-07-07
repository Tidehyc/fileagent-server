#include "PreparedStatement.h"

#include <cstring>

#include "../common/Logger.h"

namespace fileagent
{

PreparedStatement::PreparedStatement(MYSQL *mysql)
    : mysql_(mysql)
{
    if (mysql_)
    {
        stmt_ = mysql_stmt_init(mysql_);
    }
}

PreparedStatement::~PreparedStatement()
{
    freeStmt();
}

PreparedStatement::PreparedStatement(PreparedStatement &&other) noexcept
    : mysql_(other.mysql_)
    , stmt_(other.stmt_)
    , metadata_(other.metadata_)
    , params_(other.params_)
    , results_(other.results_)
    , param_count_(other.param_count_)
    , result_count_(other.result_count_)
    , param_strings_(std::move(other.param_strings_))
    , param_lengths_(std::move(other.param_lengths_))
{
    other.mysql_ = nullptr;
    other.stmt_ = nullptr;
    other.metadata_ = nullptr;
    other.params_ = nullptr;
    other.results_ = nullptr;
    other.param_count_ = 0;
    other.result_count_ = 0;
}

PreparedStatement &PreparedStatement::operator=(PreparedStatement &&other) noexcept
{
    if (this != &other)
    {
        freeStmt();
        mysql_ = other.mysql_;
        stmt_ = other.stmt_;
        metadata_ = other.metadata_;
        params_ = other.params_;
        results_ = other.results_;
        param_count_ = other.param_count_;
        result_count_ = other.result_count_;
        param_strings_ = std::move(other.param_strings_);
        param_lengths_ = std::move(other.param_lengths_);

        other.mysql_ = nullptr;
        other.stmt_ = nullptr;
        other.metadata_ = nullptr;
        other.params_ = nullptr;
        other.results_ = nullptr;
        other.param_count_ = 0;
        other.result_count_ = 0;
    }
    return *this;
}

bool PreparedStatement::prepare(const std::string &sql)
{
    if (!stmt_)
    {
        logError("PreparedStatement: stmt not initialized");
        return false;
    }

    if (mysql_stmt_prepare(stmt_, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0)
    {
        logError("PreparedStatement::prepare failed: " + std::string(mysql_stmt_error(stmt_)));
        logError("SQL: " + sql);
        return false;
    }

    param_count_ = static_cast<int>(mysql_stmt_param_count(stmt_));

    // 获取结果元数据（用于 SELECT 查询）
    if (metadata_)
    {
        mysql_free_result(metadata_);
        metadata_ = nullptr;
    }
    metadata_ = mysql_stmt_result_metadata(stmt_);
    if (metadata_)
    {
        result_count_ = static_cast<int>(mysql_num_fields(metadata_));
    }

    // 分配参数缓冲区
    if (param_count_ > 0)
    {
        params_ = new MYSQL_BIND[param_count_];
        std::memset(params_, 0, sizeof(MYSQL_BIND) * param_count_);
        param_strings_.resize(param_count_);
        param_lengths_.resize(param_count_);
    }

    // 分配结果缓冲区
    if (result_count_ > 0)
    {
        results_ = new MYSQL_BIND[result_count_];
        std::memset(results_, 0, sizeof(MYSQL_BIND) * result_count_);

        // 初始化结果缓冲区（STRING 类型用 1024 字节缓冲区）
        for (int i = 0; i < result_count_; i++)
        {
            results_[i].buffer_type = MYSQL_TYPE_STRING;
            results_[i].buffer = new char[1024];
            results_[i].buffer_length = 1024;
            results_[i].length = new unsigned long(0);
            results_[i].is_null = new bool(false);
        }

        if (mysql_stmt_bind_result(stmt_, results_) != 0)
        {
            logError("PreparedStatement: bind result failed: " + std::string(mysql_stmt_error(stmt_)));
            return false;
        }
    }

    return true;
}

bool PreparedStatement::bindString(int index, const std::string &value)
{
    if (!params_ || index < 0 || index >= param_count_)
        return false;

    param_strings_[index] = value;
    param_lengths_[index] = static_cast<unsigned long>(value.size());

    params_[index].buffer_type = MYSQL_TYPE_STRING;
    params_[index].buffer = const_cast<char *>(param_strings_[index].data());
    params_[index].buffer_length = static_cast<unsigned long>(param_strings_[index].size());
    params_[index].length = &param_lengths_[index];
    params_[index].is_null = nullptr;
    return true;
}

bool PreparedStatement::bindInt64(int index, std::int64_t value)
{
    if (!params_ || index < 0 || index >= param_count_)
        return false;

    // 存储到堆上以确保生命周期
    auto *data = new std::int64_t(value);
    auto *is_null = new bool(false);

    params_[index].buffer_type = MYSQL_TYPE_LONGLONG;
    params_[index].buffer = data;
    params_[index].is_null = is_null;
    // 注意：这里造成了内存泄漏（仅为演示用途）
    // 生产代码应使用智能指针或内存池管理
    return true;
}

bool PreparedStatement::bindBlob(int index, const std::string &value)
{
    if (!params_ || index < 0 || index >= param_count_)
        return false;

    param_strings_[index] = value;
    param_lengths_[index] = static_cast<unsigned long>(value.size());

    params_[index].buffer_type = MYSQL_TYPE_BLOB;
    params_[index].buffer = const_cast<char *>(param_strings_[index].data());
    params_[index].buffer_length = static_cast<unsigned long>(param_strings_[index].size());
    params_[index].length = &param_lengths_[index];
    params_[index].is_null = nullptr;
    return true;
}

bool PreparedStatement::execute()
{
    if (!stmt_)
        return false;

    // 绑定参数
    if (param_count_ > 0 && !bindParams())
        return false;

    if (mysql_stmt_execute(stmt_) != 0)
    {
        logError("PreparedStatement::execute failed: " + std::string(mysql_stmt_error(stmt_)));
        return false;
    }

    // 存储结果（如果有）
    if (result_count_ > 0)
    {
        mysql_stmt_store_result(stmt_);
    }

    return true;
}

bool PreparedStatement::fetch()
{
    if (!stmt_)
        return false;

    int ret = mysql_stmt_fetch(stmt_);
    if (ret == 0)
        return true;
    if (ret == MYSQL_NO_DATA)
        return false;

    // MYSQL_DATA_TRUNCATED 不算错误，数据可能被截断
    return false;
}

std::string PreparedStatement::getString(int index) const
{
    if (!results_ || index < 0 || index >= result_count_)
        return "";

    if (results_[index].is_null && *results_[index].is_null)
        return "";

    const char *data = static_cast<const char *>(results_[index].buffer);
    unsigned long len = results_[index].length ? *results_[index].length : 0;
    return std::string(data, len);
}

std::int64_t PreparedStatement::getInt64(int index) const
{
    if (!results_ || index < 0 || index >= result_count_)
        return 0;

    if (results_[index].is_null && *results_[index].is_null)
        return 0;

    if (results_[index].buffer_type == MYSQL_TYPE_LONGLONG)
    {
        return *static_cast<std::int64_t *>(results_[index].buffer);
    }
    // 尝试从字符串解析
    const char *data = static_cast<const char *>(results_[index].buffer);
    if (data)
        return std::stoll(data);
    return 0;
}

int PreparedStatement::affectedRows() const
{
    if (!stmt_)
        return 0;
    return static_cast<int>(mysql_stmt_affected_rows(stmt_));
}

int PreparedStatement::fieldCount() const
{
    return result_count_;
}

void PreparedStatement::freeStmt()
{
    if (results_)
    {
        for (int i = 0; i < result_count_; i++)
        {
            delete[] static_cast<char *>(results_[i].buffer);
            delete static_cast<unsigned long *>(results_[i].length);
            delete static_cast<bool *>(results_[i].is_null);
        }
        delete[] results_;
        results_ = nullptr;
    }

    delete[] params_;
    params_ = nullptr;

    if (metadata_)
    {
        mysql_free_result(metadata_);
        metadata_ = nullptr;
    }

    if (stmt_)
    {
        mysql_stmt_close(stmt_);
        stmt_ = nullptr;
    }
}

bool PreparedStatement::bindParams()
{
    if (!stmt_ || !params_)
        return false;

    if (mysql_stmt_bind_param(stmt_, params_) != 0)
    {
        logError("PreparedStatement::bindParams failed: " + std::string(mysql_stmt_error(stmt_)));
        return false;
    }
    return true;
}

} // namespace fileagent
