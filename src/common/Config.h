#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace fileagent
{

    // 连接池配置
    struct ConnectionPoolConfig
    {
        size_t min_size = 4;
        size_t max_size = 20;
        std::chrono::seconds timeout{5};
        std::chrono::seconds idle_timeout{300};
        std::chrono::seconds heartbeat_interval{60};
    };

    // 服务器配置
    struct ServerConfig
    {
        std::string host = "0.0.0.0";
        uint16_t port = 10086;
        int threads = 4;
        int max_connections = 1024;
    };

    // 数据库配置
    struct DatabaseConfig
    {
        std::string host = "127.0.0.1";
        uint16_t port = 3306;
        std::string user;
        std::string password;
        std::string database;
        ConnectionPoolConfig pool;
    };

    // Redis 缓存配置
    struct RedisCacheConfig
    {
        std::string host = "127.0.0.1";
        uint16_t port = 6379;
    };

    // LRU 缓存配置
    struct LruCacheConfig
    {
        size_t capacity = 10000;
    };

    // 缓存配置
    struct CacheConfig
    {
        std::string type = "lru"; // "redis" 或 "lru"
        RedisCacheConfig redis;
        LruCacheConfig lru;
    };

    // LLM 配置
    struct LlmConfig
    {
        std::string model_path;
        int num_threads = 4;
        int context_window = 2048;
    };

    // 完整的应用配置
    struct AppConfig
    {
        ServerConfig server;
        DatabaseConfig database;
        CacheConfig cache;
        LlmConfig llm;

        // 从配置文件加载
        bool loadFromFile(const std::string &config_path);

        // 验证配置的合法性
        bool validate() const;

        // 转换为可读的字符串表示（用于日志）
        std::string toString() const;
    };

} // namespace fileagent