#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <hiredis/hiredis.h>

#include "CacheInterface.h"
#include "Config.h"
#include "../dao/Models.h"

namespace fileagent
{

    /**
     * @brief hiredis 客户端封装
     *
     * 管理 Redis 连接，提供基础的 set/get/del/exists 操作。
     * 支持连接自动重连和超时。
     */
    class RedisClient
    {
    public:
        explicit RedisClient(const RedisCacheConfig &config);
        ~RedisClient();

        RedisClient(const RedisClient &) = delete;
        RedisClient &operator=(const RedisClient &) = delete;

        bool set(const std::string &key, const std::string &value, int ttl_seconds = 0);
        std::optional<std::string> get(const std::string &key);
        bool del(const std::string &key);
        bool exists(const std::string &key);
        bool isConnected() const;

    private:
        bool reconnect();
        void freeContext();

        std::string host_;
        int port_;
        redisContext *context_{nullptr};
        int timeout_sec_{5};
    };

    // ──────────────────────────────────────────────
    // Redis 缓存实现（FileRecord by ID）
    // ──────────────────────────────────────────────

    class RedisFileCache : public CacheInterface<std::int64_t, FileRecord>
    {
    public:
        explicit RedisFileCache(RedisClient &client);
        void put(const std::int64_t &key, FileRecord value) override;
        std::optional<FileRecord> get(const std::int64_t &key) override;
        bool remove(const std::int64_t &key) override;
        void clear() override;
        size_t size() const override;

        // 序列化工具（公开给其他缓存类复用）
        static std::string serializeRecord(const FileRecord &record);
        static std::optional<FileRecord> deserializeRecord(const std::string &data);

    private:
        static std::string makeKey(std::int64_t id);

        RedisClient &client_;
    };

    // ──────────────────────────────────────────────
    // Redis 缓存实现（FileRecord by hash）
    // ──────────────────────────────────────────────

    class RedisFileHashCache : public CacheInterface<std::string, FileRecord>
    {
    public:
        explicit RedisFileHashCache(RedisClient &client);
        void put(const std::string &key, FileRecord value) override;
        std::optional<FileRecord> get(const std::string &key) override;
        bool remove(const std::string &key) override;
        void clear() override;
        size_t size() const override;

    private:
        static std::string makeKey(const std::string &hash);
        static std::string serialize(const FileRecord &record);
        static std::optional<FileRecord> deserialize(const std::string &data);

        RedisClient &client_;
    };

    // ──────────────────────────────────────────────
    // Redis 缓存实现（SessionRecord by token）
    // ──────────────────────────────────────────────

    class RedisSessionCache : public CacheInterface<std::string, SessionRecord>
    {
    public:
        explicit RedisSessionCache(RedisClient &client);
        void put(const std::string &key, SessionRecord value) override;
        std::optional<SessionRecord> get(const std::string &key) override;
        bool remove(const std::string &key) override;
        void clear() override;
        size_t size() const override;

    private:
        static std::string makeKey(const std::string &token);
        static std::string serialize(const SessionRecord &record);
        static std::optional<SessionRecord> deserialize(const std::string &data);

        RedisClient &client_;
    };

} // namespace fileagent
