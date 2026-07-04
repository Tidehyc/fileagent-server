#pragma once

#include <memory>
#include <string>

#include "CacheInterface.h"
#include "Config.h"
#include "../dao/Models.h"

namespace fileagent
{

    class RedisClient;

    /**
     * @brief 缓存管理器单例
     *
     * 根据 CacheConfig::type 运行时切换 LRU 内存缓存 / Redis 缓存。
     */
    class CacheManager
    {
    public:
        static CacheManager &instance();

        /**
         * @brief 根据配置初始化缓存
         */
        void init(const CacheConfig &config);

        bool initialized() const { return initialized_; }

        // 缓存类型
        const std::string &type() const { return type_; }

        // ─── 缓存访问接口 ─────────────────────────

        CacheInterface<std::int64_t, FileRecord> &fileCacheById()
        {
            return *file_cache_by_id_;
        }

        CacheInterface<std::string, FileRecord> &fileCacheByHash()
        {
            return *file_cache_by_hash_;
        }

        CacheInterface<std::string, SessionRecord> &sessionCache()
        {
            return *session_cache_;
        }

        CacheManager(const CacheManager &) = delete;
        CacheManager &operator=(const CacheManager &) = delete;

    private:
        CacheManager() = default;

        bool initialized_{false};
        std::string type_{"lru"};

        std::unique_ptr<CacheInterface<std::int64_t, FileRecord>> file_cache_by_id_;
        std::unique_ptr<CacheInterface<std::string, FileRecord>> file_cache_by_hash_;
        std::unique_ptr<CacheInterface<std::string, SessionRecord>> session_cache_;

        // Redis 客户端（仅 Redis 模式使用）
        std::unique_ptr<RedisClient> redis_client_;
    };

} // namespace fileagent
