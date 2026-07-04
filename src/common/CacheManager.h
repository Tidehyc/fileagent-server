#pragma once

#include <memory>

#include "Config.h"
#include "LruCache.h"
#include "../dao/Models.h"

namespace fileagent
{

    /**
     * @brief 缓存管理器单例
     *
     * 持有多个 LRU 缓存实例，根据 CacheConfig 初始化。
     * 当前仅支持 LRU 模式，后续可扩展 Redis。
     */
    class CacheManager
    {
    public:
        static CacheManager &instance();

        /**
         * @brief 根据配置初始化缓存
         */
        void init(const CacheConfig &config);

        /**
         * @brief 是否已初始化
         */
        bool initialized() const { return initialized_; }

        /**
         * @brief 获取文件记录缓存
         */
        LruCache<std::int64_t, FileRecord> &fileCacheById()
        {
            return *file_cache_by_id_;
        }

        /**
         * @brief 获取文件记录哈希缓存
         */
        LruCache<std::string, FileRecord> &fileCacheByHash()
        {
            return *file_cache_by_hash_;
        }

        /**
         * @brief 获取 session 缓存
         */
        LruCache<std::string, SessionRecord> &sessionCache()
        {
            return *session_cache_;
        }

        CacheManager(const CacheManager &) = delete;
        CacheManager &operator=(const CacheManager &) = delete;

    private:
        CacheManager() = default;

        bool initialized_{false};

        std::unique_ptr<LruCache<std::int64_t, FileRecord>> file_cache_by_id_;
        std::unique_ptr<LruCache<std::string, FileRecord>> file_cache_by_hash_;
        std::unique_ptr<LruCache<std::string, SessionRecord>> session_cache_;
    };

} // namespace fileagent
