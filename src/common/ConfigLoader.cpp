#include "ConfigLoader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <yaml-cpp/yaml.h>
#include "Logger.h"

namespace fileagent
{

    bool AppConfig::loadFromFile(const std::string &config_path)
    {
        try
        {
            YAML::Node config = YAML::LoadFile(config_path);

            // 加载服务器配置
            if (config["server"])
            {
                YAML::Node srv = config["server"];
                server.host = srv["host"].as<std::string>(server.host);
                server.port = srv["port"].as<uint16_t>(server.port);
                server.threads = srv["threads"].as<int>(server.threads);
                server.max_connections = srv["max_connections"].as<int>(server.max_connections);
            }

            // 加载数据库配置
            if (config["database"])
            {
                YAML::Node db = config["database"];
                database.host = db["host"].as<std::string>(database.host);
                database.port = db["port"].as<uint16_t>(database.port);
                database.user = db["user"].as<std::string>("");
                database.password = db["password"].as<std::string>("");
                database.database = db["database"].as<std::string>("");

                // 加载连接池配置
                if (db["pool"])
                {
                    YAML::Node pool = db["pool"];
                    database.pool.min_size = pool["min_size"].as<size_t>(database.pool.min_size);
                    database.pool.max_size = pool["max_size"].as<size_t>(database.pool.max_size);
                    database.pool.timeout = std::chrono::seconds(
                        pool["timeout"].as<int>(database.pool.timeout.count()));
                    database.pool.idle_timeout = std::chrono::seconds(
                        pool["idle_timeout"].as<int>(database.pool.idle_timeout.count()));
                    database.pool.heartbeat_interval = std::chrono::seconds(
                        pool["heartbeat_interval"].as<int>(database.pool.heartbeat_interval.count()));
                }
            }

            // 加载缓存配置
            if (config["cache"])
            {
                YAML::Node c = config["cache"];
                cache.type = c["type"].as<std::string>(cache.type);

                if (c["redis"])
                {
                    YAML::Node redis = c["redis"];
                    cache.redis.host = redis["host"].as<std::string>(cache.redis.host);
                    cache.redis.port = redis["port"].as<uint16_t>(cache.redis.port);
                }

                if (c["lru"])
                {
                    YAML::Node lru = c["lru"];
                    cache.lru.capacity = lru["capacity"].as<size_t>(cache.lru.capacity);
                }
            }

            // 加载 LLM 配置
            if (config["llm"])
            {
                YAML::Node llm_cfg = config["llm"];
                llm.provider = llm_cfg["provider"].as<std::string>(llm.provider);
                llm.api_key = llm_cfg["api_key"].as<std::string>(llm.api_key);
                llm.api_base = llm_cfg["api_base"].as<std::string>(llm.api_base);
                llm.model = llm_cfg["model"].as<std::string>(llm.model);
                llm.model_path = llm_cfg["model_path"].as<std::string>("");
                llm.num_threads = llm_cfg["num_threads"].as<int>(llm.num_threads);
                llm.context_window = llm_cfg["context_window"].as<int>(llm.context_window);
            }

            logInfo("Configuration loaded successfully from: " + config_path);
            return true;
        }
        catch (const YAML::Exception &e)
        {
            logError("YAML parsing error: " + std::string(e.what()));
            return false;
        }
        catch (const std::exception &e)
        {
            logError("Failed to load config: " + std::string(e.what()));
            return false;
        }
    }

    bool AppConfig::validate() const
    {
        // 验证服务器配置
        if (server.port == 0)
        {
            logError("Invalid server port: " + std::to_string(server.port));
            return false;
        }

        if (server.threads <= 0 || server.threads > 256)
        {
            logError("Invalid thread count: " + std::to_string(server.threads));
            return false;
        }

        // 验证数据库配置
        if (database.user.empty() || database.password.empty() || database.database.empty())
        {
            logError("Database user, password, or database name is empty");
            return false;
        }

        if (database.pool.min_size == 0 || database.pool.max_size < database.pool.min_size)
        {
            logError("Invalid pool size: min=" + std::to_string(database.pool.min_size) +
                     ", max=" + std::to_string(database.pool.max_size));
            return false;
        }

        // 验证缓存配置
        if (cache.type != "redis" && cache.type != "lru")
        {
            logError("Invalid cache type: " + cache.type);
            return false;
        }

        if (cache.lru.capacity == 0)
        {
            logError("Invalid LRU cache capacity");
            return false;
        }

        return true;
    }

    std::string AppConfig::toString() const
    {
        std::stringstream ss;
        ss << "AppConfig {\n"
           << "  Server: host=" << server.host << ", port=" << server.port
           << ", threads=" << server.threads << ", max_connections=" << server.max_connections << "\n"
           << "  Database: host=" << database.host << ", port=" << database.port
           << ", user=" << database.user << ", database=" << database.database << "\n"
           << "    Pool: min_size=" << database.pool.min_size << ", max_size=" << database.pool.max_size
           << ", timeout=" << database.pool.timeout.count() << "s"
           << ", idle_timeout=" << database.pool.idle_timeout.count() << "s\n"
           << "  Cache: type=" << cache.type;

        if (cache.type == "redis")
        {
            ss << " (redis=" << cache.redis.host << ":" << cache.redis.port << ")";
        }
        else
        {
            ss << " (lru_capacity=" << cache.lru.capacity << ")";
        }

        ss << "\n"
           << "  LLM: provider=" << llm.provider << ", model=" << llm.model
           << ", api_base=" << llm.api_base << ", model_path=" << llm.model_path << "\n"
           << "}";

        return ss.str();
    }

} // namespace fileagent
