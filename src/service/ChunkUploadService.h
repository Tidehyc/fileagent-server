#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>

namespace fileagent
{

    /**
     * @brief 分片上传会话
     *
     * 存储在内存中，不入库。超时未完成会自动清理。
     */
    struct ChunkUploadSession
    {
        std::string upload_id;
        std::int64_t user_id;
        std::string filename;
        std::int64_t file_size;
        int total_chunks;
        int chunk_size;
        std::set<int> received;
        std::string file_hash;
        std::chrono::steady_clock::time_point created_at;
    };

    /**
     * @brief 分片上传管理器（线程安全单例）
     *
     * 管理所有活跃的分片上传会话，包含自动超时清理。
     */
    class ChunkUploadManager
    {
    public:
        static ChunkUploadManager &instance();

        /**
         * @brief 创建新分片上传会话
         * @return upload_id
         */
        std::string createSession(std::int64_t user_id,
                                   const std::string &filename,
                                   std::int64_t file_size,
                                   int total_chunks,
                                   int chunk_size);

        /**
         * @brief 获取会话（线程安全）
         */
        std::shared_ptr<ChunkUploadSession> getSession(const std::string &upload_id);

        /**
         * @brief 标记分片已接收
         */
        bool markChunkReceived(const std::string &upload_id, int chunk_index);

        /**
         * @brief 移除会话
         */
        void removeSession(const std::string &upload_id);

        ChunkUploadManager(const ChunkUploadManager &) = delete;
        ChunkUploadManager &operator=(const ChunkUploadManager &) = delete;

    private:
        ChunkUploadManager();
        ~ChunkUploadManager();

        void cleanupLoop();

        std::unordered_map<std::string, std::shared_ptr<ChunkUploadSession>> sessions_;
        mutable std::mutex mutex_;
        std::thread cleanup_thread_;
        std::atomic<bool> running_{false};
    };

} // namespace fileagent
