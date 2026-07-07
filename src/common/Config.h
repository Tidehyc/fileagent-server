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

    // 存储配置
    struct StorageConfig
    {
        std::string type = "local"; // "local" 或 "fastdfs"
        std::string fastdfs_conf = "/etc/fdfs/client.conf"; // FastDFS client.conf 路径
        std::string fastdfs_data_path = ""; // FastDFS 数据目录（Nginx X-Accel-Redirect 用）
    };

    // LLM 配置
    struct LlmConfig
    {
        std::string provider = "ollama";     // "ollama" / "openai" / "anthropic" / "google" / "local"
        std::string api_key;                 // 云端 API Key（openai/anthropic/google）
        std::string api_base;                 // 自定义 API 地址（可选）
        std::string model = "llama3.2";       // 模型名（按 provider 不同）
        std::string embedding_model;          // Embedding 模型名（如 "nomic-embed-text"，空则用 model）
        std::string model_path;               // 本地 GGUF 路径（provider=local 时使用）
        int num_threads = 4;
        int context_window = 2048;
    };

    // 完整的应用配置
    struct AppConfig
    {
        ServerConfig server;
        DatabaseConfig database;
        CacheConfig cache;
        StorageConfig storage;
        LlmConfig llm;

        // 从配置文件加载
        bool loadFromFile(const std::string &config_path);

        // 验证配置的合法性
        bool validate() const;

        // 转换为可读的字符串表示（用于日志）
        std::string toString() const;
    };

} // namespace fileagent