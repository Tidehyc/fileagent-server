#include "CacheManager.h"

#include "Logger.h"
#include "LruCache.h"
#include "RedisCache.h"

namespace fileagent
{

    CacheManager &CacheManager::instance()
    {
        static CacheManager manager;
        return manager;
    }

    void CacheManager::init(const CacheConfig &config)
    {
        if (initialized_)
        {
            return;
        }

        type_ = config.type;

        if (config.type == "redis")
        {
            logInfo("CacheManager: initializing Redis cache (host=" + config.redis.host +
                    ":" + std::to_string(config.redis.port) + ")");

            redis_client_ = std::make_unique<RedisClient>(config.redis);

            if (redis_client_->isConnected())
            {
                file_cache_by_id_ = std::make_unique<RedisFileCache>(*redis_client_);
                file_cache_by_hash_ = std::make_unique<RedisFileHashCache>(*redis_client_);
                session_cache_ = std::make_unique<RedisSessionCache>(*redis_client_);
                logInfo("CacheManager: Redis cache active");
            }
            else
            {
                logWarn("CacheManager: Redis unavailable, falling back to LRU cache");
                file_cache_by_id_ = std::make_unique<LruCache<std::int64_t, FileRecord>>(
                    config.lru.capacity);
                file_cache_by_hash_ = std::make_unique<LruCache<std::string, FileRecord>>(
                    config.lru.capacity);
                session_cache_ = std::make_unique<LruCache<std::string, SessionRecord>>(
                    config.lru.capacity);
                type_ = "lru";
            }
        }
        else
        {
            logInfo("CacheManager: initializing LRU cache (capacity=" +
                    std::to_string(config.lru.capacity) + ")");

            file_cache_by_id_ = std::make_unique<LruCache<std::int64_t, FileRecord>>(
                config.lru.capacity);
            file_cache_by_hash_ = std::make_unique<LruCache<std::string, FileRecord>>(
                config.lru.capacity);
            session_cache_ = std::make_unique<LruCache<std::string, SessionRecord>>(
                config.lru.capacity);
        }

        initialized_ = true;
    }

} // namespace fileagent
