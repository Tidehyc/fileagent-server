#include "RedisCache.h"

#include "Logger.h"

namespace fileagent
{

    // ──────────────────────────────────────────────
    // RedisClient
    // ──────────────────────────────────────────────

    RedisClient::RedisClient(const RedisCacheConfig &config)
        : host_(config.host), port_(config.port)
    {
        struct timeval tv = {timeout_sec_, 0};
        context_ = redisConnectWithTimeout(host_.c_str(), port_, tv);
        if (context_ == nullptr || context_->err)
        {
            logWarn("RedisClient: failed to connect to " + host_ + ":" +
                    std::to_string(port_) + " - " +
                    (context_ ? context_->errstr : "out of memory"));
            logWarn("RedisClient: caching disabled, falling back to no-cache mode");
            if (context_)
            {
                freeContext();
            }
        }
        else
        {
            logInfo("RedisClient: connected to " + host_ + ":" + std::to_string(port_));
        }
    }

    RedisClient::~RedisClient()
    {
        freeContext();
    }

    bool RedisClient::reconnect()
    {
        freeContext();
        struct timeval tv = {timeout_sec_, 0};
        context_ = redisConnectWithTimeout(host_.c_str(), port_, tv);
        return context_ != nullptr && context_->err == 0;
    }

    void RedisClient::freeContext()
    {
        if (context_)
        {
            redisFree(context_);
            context_ = nullptr;
        }
    }

    bool RedisClient::isConnected() const
    {
        return context_ != nullptr && context_->err == 0;
    }

    bool RedisClient::set(const std::string &key, const std::string &value, int ttl_seconds)
    {
        if (!isConnected())
            return false;

        redisReply *reply = nullptr;
        if (ttl_seconds > 0)
        {
            reply = static_cast<redisReply *>(
                redisCommand(context_, "SETEX %s %d %s", key.c_str(), ttl_seconds, value.c_str()));
        }
        else
        {
            reply = static_cast<redisReply *>(
                redisCommand(context_, "SET %s %s", key.c_str(), value.c_str()));
        }

        if (reply == nullptr)
        {
            logError("RedisClient::set: command failed (connection lost?)");
            return false;
        }

        bool ok = (reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "OK");
        freeReplyObject(reply);
        return ok;
    }

    std::optional<std::string> RedisClient::get(const std::string &key)
    {
        if (!isConnected())
            return std::nullopt;

        redisReply *reply = static_cast<redisReply *>(
            redisCommand(context_, "GET %s", key.c_str()));

        if (reply == nullptr)
            return std::nullopt;

        std::optional<std::string> result;
        if (reply->type == REDIS_REPLY_STRING)
        {
            result = std::string(reply->str, reply->len);
        }
        freeReplyObject(reply);
        return result;
    }

    bool RedisClient::del(const std::string &key)
    {
        if (!isConnected())
            return false;

        redisReply *reply = static_cast<redisReply *>(
            redisCommand(context_, "DEL %s", key.c_str()));

        if (reply == nullptr)
            return false;

        bool ok = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
        freeReplyObject(reply);
        return ok;
    }

    bool RedisClient::zincrby(const std::string &key, const std::string &member, double increment)
    {
        if (!isConnected())
            return false;

        redisReply *reply = static_cast<redisReply *>(
            redisCommand(context_, "ZINCRBY %s %f %s", key.c_str(), increment, member.c_str()));

        if (reply == nullptr)
            return false;

        bool ok = (reply->type == REDIS_REPLY_STRING);
        freeReplyObject(reply);
        return ok;
    }

    std::vector<std::pair<std::string, double>> RedisClient::zrevrange(const std::string &key, int start, int stop)
    {
        std::vector<std::pair<std::string, double>> result;

        if (!isConnected())
            return result;

        redisReply *reply = static_cast<redisReply *>(
            redisCommand(context_, "ZREVRANGE %s %d %d WITHSCORES", key.c_str(), start, stop));

        if (reply == nullptr || reply->type != REDIS_REPLY_ARRAY)
        {
            if (reply) freeReplyObject(reply);
            return result;
        }

        for (size_t i = 0; i + 1 < reply->elements; i += 2)
        {
            if (reply->element[i]->type == REDIS_REPLY_STRING &&
                reply->element[i + 1]->type == REDIS_REPLY_STRING)
            {
                std::string member(reply->element[i]->str, reply->element[i]->len);
                double score = std::stod(std::string(reply->element[i + 1]->str, reply->element[i + 1]->len));
                result.emplace_back(std::move(member), score);
            }
        }

        freeReplyObject(reply);
        return result;
    }

    bool RedisClient::exists(const std::string &key)
    {
        if (!isConnected())
            return false;

        redisReply *reply = static_cast<redisReply *>(
            redisCommand(context_, "EXISTS %s", key.c_str()));

        if (reply == nullptr)
            return false;

        bool ok = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
        freeReplyObject(reply);
        return ok;
    }

    // ──────────────────────────────────────────────
    // RedisFileCache — FileRecord by ID
    // ──────────────────────────────────────────────

    RedisFileCache::RedisFileCache(RedisClient &client)
        : client_(client) {}

    std::string RedisFileCache::makeKey(std::int64_t id)
    {
        return "file:id:" + std::to_string(id);
    }

    std::string RedisFileCache::serializeRecord(const FileRecord &record)
    {
        // 简单的 JSON 序列化（避免引入额外 JSON 库依赖）
        std::string json;
        json += "{";
        json += "\"id\":" + std::to_string(record.id) + ",";
        json += "\"user_id\":" + std::to_string(record.user_id) + ",";
        json += "\"filename\":\"" + record.filename + "\",";
        json += "\"file_hash\":\"" + record.file_hash + "\",";
        json += "\"file_size\":" + std::to_string(record.file_size) + ",";
        json += "\"storage_path\":\"" + record.storage_path + "\",";
        json += "\"status\":" + std::to_string(record.status) + ",";
        json += "\"created_at\":\"" + record.created_at + "\"";
        json += "}";
        return json;
    }

    std::optional<FileRecord> RedisFileCache::deserializeRecord(const std::string &data)
    {
        // 简易 JSON 解析（手动查找键值对）
        FileRecord record;

        auto findInt = [&](const std::string &key) -> std::int64_t
        {
            auto pos = data.find("\"" + key + "\":");
            if (pos == std::string::npos)
                return 0;
            pos += key.size() + 3; // 跳过 "key":
            char *end = nullptr;
            return std::strtoll(data.c_str() + pos, &end, 10);
        };

        auto findStr = [&](const std::string &key) -> std::string
        {
            auto pos = data.find("\"" + key + "\":\"");
            if (pos == std::string::npos)
                return "";
            pos += key.size() + 4; // 跳过 "key":"
            auto end = data.find("\"", pos);
            if (end == std::string::npos)
                return "";
            return data.substr(pos, end - pos);
        };

        record.id = findInt("id");
        record.user_id = findInt("user_id");
        record.filename = findStr("filename");
        record.file_hash = findStr("file_hash");
        record.file_size = findInt("file_size");
        record.storage_path = findStr("storage_path");
        record.status = static_cast<int>(findInt("status"));
        record.created_at = findStr("created_at");

        return record;
    }

    void RedisFileCache::put(const std::int64_t &key, FileRecord value)
    {
        // TTL: 1 小时
        client_.set(makeKey(key), serializeRecord(value), 3600);
    }

    std::optional<FileRecord> RedisFileCache::get(const std::int64_t &key)
    {
        auto data = client_.get(makeKey(key));
        if (!data.has_value())
            return std::nullopt;
        return deserializeRecord(*data);
    }

    bool RedisFileCache::remove(const std::int64_t &key)
    {
        return client_.del(makeKey(key));
    }

    void RedisFileCache::clear()
    {
        // Redis 无简单通配删除，记录警告
        logWarn("RedisFileCache::clear: not implemented for Redis");
    }

    size_t RedisFileCache::size() const
    {
        // Redis 无法直接获取匹配 key 的数量
        return 0;
    }

    // ──────────────────────────────────────────────
    // RedisFileHashCache — FileRecord by hash
    // ──────────────────────────────────────────────

    RedisFileHashCache::RedisFileHashCache(RedisClient &client)
        : client_(client) {}

    std::string RedisFileHashCache::makeKey(const std::string &hash)
    {
        return "file:hash:" + hash;
    }

    std::string RedisFileHashCache::serialize(const FileRecord &record)
    {
        return RedisFileCache::serializeRecord(record);
    }

    std::optional<FileRecord> RedisFileHashCache::deserialize(const std::string &data)
    {
        return RedisFileCache::deserializeRecord(data);
    }

    void RedisFileHashCache::put(const std::string &key, FileRecord value)
    {
        client_.set(makeKey(key), serialize(value), 3600);
    }

    std::optional<FileRecord> RedisFileHashCache::get(const std::string &key)
    {
        auto data = client_.get(makeKey(key));
        if (!data.has_value())
            return std::nullopt;
        return deserialize(*data);
    }

    bool RedisFileHashCache::remove(const std::string &key)
    {
        return client_.del(makeKey(key));
    }

    void RedisFileHashCache::clear()
    {
        logWarn("RedisFileHashCache::clear: not implemented for Redis");
    }

    size_t RedisFileHashCache::size() const
    {
        return 0;
    }

    // ──────────────────────────────────────────────
    // RedisSessionCache — SessionRecord by token
    // ──────────────────────────────────────────────

    RedisSessionCache::RedisSessionCache(RedisClient &client)
        : client_(client) {}

    std::string RedisSessionCache::makeKey(const std::string &token)
    {
        return "session:" + token;
    }

    std::string RedisSessionCache::serialize(const SessionRecord &record)
    {
        std::string json;
        json += "{";
        json += "\"id\":" + std::to_string(record.id) + ",";
        json += "\"user_id\":" + std::to_string(record.user_id) + ",";
        json += "\"session_token\":\"" + record.session_token + "\",";
        json += "\"expires_at\":\"" + record.expires_at + "\",";
        json += "\"created_at\":\"" + record.created_at + "\"";
        json += "}";
        return json;
    }

    std::optional<SessionRecord> RedisSessionCache::deserialize(const std::string &data)
    {
        SessionRecord record;

        auto findInt = [&](const std::string &key) -> std::int64_t
        {
            auto pos = data.find("\"" + key + "\":");
            if (pos == std::string::npos)
                return 0;
            pos += key.size() + 3;
            char *end = nullptr;
            return std::strtoll(data.c_str() + pos, &end, 10);
        };

        auto findStr = [&](const std::string &key) -> std::string
        {
            auto pos = data.find("\"" + key + "\":\"");
            if (pos == std::string::npos)
                return "";
            pos += key.size() + 4;
            auto end = data.find("\"", pos);
            if (end == std::string::npos)
                return "";
            return data.substr(pos, end - pos);
        };

        record.id = findInt("id");
        record.user_id = findInt("user_id");
        record.session_token = findStr("session_token");
        record.expires_at = findStr("expires_at");
        record.created_at = findStr("created_at");

        return record;
    }

    void RedisSessionCache::put(const std::string &key, SessionRecord value)
    {
        // TTL: 7 天（与 session 过期一致）
        client_.set(makeKey(key), serialize(value), 86400 * 7);
    }

    std::optional<SessionRecord> RedisSessionCache::get(const std::string &key)
    {
        auto data = client_.get(makeKey(key));
        if (!data.has_value())
            return std::nullopt;
        return deserialize(*data);
    }

    bool RedisSessionCache::remove(const std::string &key)
    {
        return client_.del(makeKey(key));
    }

    void RedisSessionCache::clear()
    {
        logWarn("RedisSessionCache::clear: not implemented for Redis");
    }

    size_t RedisSessionCache::size() const
    {
        return 0;
    }

} // namespace fileagent
