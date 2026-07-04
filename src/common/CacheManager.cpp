#include "CacheManager.h"

#include "Logger.h"

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

        if (config.type == "lru")
        {
            file_cache_by_id_ = std::make_unique<LruCache<std::int64_t, FileRecord>>(
                config.lru.capacity);

            file_cache_by_hash_ = std::make_unique<LruCache<std::string, FileRecord>>(
                config.lru.capacity);

            session_cache_ = std::make_unique<LruCache<std::string, SessionRecord>>(
                config.lru.capacity);

            logInfo("CacheManager initialized: type=lru, capacity=" +
                    std::to_string(config.lru.capacity));
        }
        else if (config.type == "redis")
        {
            logWarn("CacheManager: Redis cache type configured but not yet implemented, "
                    "falling back to LRU");

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
