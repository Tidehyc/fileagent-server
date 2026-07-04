#include "ChunkUploadService.h"

#include <algorithm>
#include <vector>

#include "../common/Hash.h"
#include "../common/Logger.h"

namespace fileagent
{

    ChunkUploadManager &ChunkUploadManager::instance()
    {
        static ChunkUploadManager manager;
        return manager;
    }

    ChunkUploadManager::ChunkUploadManager()
    {
        running_ = true;
        cleanup_thread_ = std::thread(&ChunkUploadManager::cleanupLoop, this);
    }

    ChunkUploadManager::~ChunkUploadManager()
    {
        running_ = false;
        if (cleanup_thread_.joinable())
        {
            cleanup_thread_.join();
        }
    }

    std::string ChunkUploadManager::createSession(std::int64_t user_id,
                                                    const std::string &filename,
                                                    std::int64_t file_size,
                                                    int total_chunks,
                                                    int chunk_size)
    {
        // 生成唯一 upload_id（32 字符 hex）
        std::string upload_id = PasswordHasher::generateToken(16);
        auto now = std::chrono::steady_clock::now();

        auto session = std::make_shared<ChunkUploadSession>();
        session->upload_id = upload_id;
        session->user_id = user_id;
        session->filename = filename;
        session->file_size = file_size;
        session->total_chunks = total_chunks;
        session->chunk_size = chunk_size;
        session->created_at = now;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            sessions_[upload_id] = std::move(session);
        }

        logInfo("ChunkUpload: session created id=" + upload_id +
                " filename=" + filename +
                " chunks=" + std::to_string(total_chunks) +
                " size=" + std::to_string(file_size));

        return upload_id;
    }

    std::shared_ptr<ChunkUploadSession> ChunkUploadManager::getSession(const std::string &upload_id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(upload_id);
        if (it == sessions_.end())
        {
            return nullptr;
        }
        return it->second;
    }

    bool ChunkUploadManager::markChunkReceived(const std::string &upload_id, int chunk_index)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(upload_id);
        if (it == sessions_.end())
        {
            return false;
        }
        it->second->received.insert(chunk_index);
        return true;
    }

    void ChunkUploadManager::removeSession(const std::string &upload_id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.erase(upload_id);
    }

    void ChunkUploadManager::cleanupLoop()
    {
        while (running_)
        {
            std::this_thread::sleep_for(std::chrono::seconds(30));

            auto now = std::chrono::steady_clock::now();
            constexpr auto max_age = std::chrono::minutes(30);

            std::lock_guard<std::mutex> lock(mutex_);

            std::vector<std::string> to_remove;
            for (const auto &[id, session] : sessions_)
            {
                if (now - session->created_at > max_age)
                {
                    to_remove.push_back(id);
                }
            }

            for (const auto &id : to_remove)
            {
                logWarn("ChunkUpload: session expired id=" + id);
                sessions_.erase(id);
                // 临时文件由操作系统清理或在下次访问时检测
            }

            if (!to_remove.empty())
            {
                logInfo("ChunkUpload: cleaned " + std::to_string(to_remove.size()) +
                        " expired sessions");
            }
        }
    }

} // namespace fileagent
