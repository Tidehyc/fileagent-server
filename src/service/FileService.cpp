#include "FileService.h"

#include "../common/CacheManager.h"
#include "../common/Logger.h"
#include "../common/StringUtil.h"
#include "../dao/FileDao.h"

namespace fileagent
{

    bool FileService::validateRecord(const FileRecord &record, std::string &message) const
    {
        if (record.user_id <= 0)
        {
            message = "user id is invalid";
            return false;
        }

        if (record.filename.empty())
        {
            message = "filename is empty";
            return false;
        }

        if (record.file_hash.empty())
        {
            message = "file hash is empty";
            return false;
        }

        if (record.storage_path.empty())
        {
            message = "storage path is empty";
            return false;
        }

        if (record.file_size < 0)
        {
            message = "file size is invalid";
            return false;
        }

        return true;
    }

    ServiceResult<FileRecord> FileService::createFile(const FileRecord &record)
    {
        FileRecord normalized = record;
        normalized.filename = trimCopy(normalized.filename);
        normalized.file_hash = trimCopy(normalized.file_hash);
        normalized.storage_path = trimCopy(normalized.storage_path);

        std::string message;
        if (!validateRecord(normalized, message))
        {
            return ServiceResult<FileRecord>::fail(message);
        }

        FileDao file_dao;
        if (auto existing = file_dao.findByHash(normalized.file_hash); existing.has_value())
        {
            return ServiceResult<FileRecord>::ok(*existing, "file already exists");
        }

        if (!file_dao.createFile(normalized))
        {
            logError("failed to create file record");
            return ServiceResult<FileRecord>::fail("failed to create file");
        }

        auto created_file = file_dao.findByHash(normalized.file_hash);
        if (!created_file.has_value())
        {
            return ServiceResult<FileRecord>::fail("file created but cannot be reloaded");
        }

        // 写入缓存
        CacheManager::instance().fileCacheById().put(created_file->id, *created_file);
        CacheManager::instance().fileCacheByHash().put(created_file->file_hash, *created_file);

        return ServiceResult<FileRecord>::ok(*created_file, "create file success");
    }

    ServiceResult<FileRecord> FileService::getFileById(std::int64_t file_id)
    {
        if (file_id <= 0)
        {
            return ServiceResult<FileRecord>::fail("file id is invalid");
        }

        // 查缓存
        auto &cache = CacheManager::instance().fileCacheById();
        auto cached = cache.get(file_id);
        if (cached.has_value())
        {
            return ServiceResult<FileRecord>::ok(*cached, "cache hit");
        }

        // 未命中，查库
        FileDao file_dao;
        auto file = file_dao.findById(file_id);
        if (!file.has_value())
        {
            return ServiceResult<FileRecord>::fail("file not found");
        }

        // 写入缓存
        cache.put(file->id, *file);

        return ServiceResult<FileRecord>::ok(*file, "query success");
    }

    ServiceResult<FileRecord> FileService::getFileByHash(const std::string &file_hash)
    {
        std::string normalized_hash = trimCopy(file_hash);
        if (normalized_hash.empty())
        {
            return ServiceResult<FileRecord>::fail("file hash is empty");
        }

        // 查缓存
        auto &cache = CacheManager::instance().fileCacheByHash();
        auto cached = cache.get(normalized_hash);
        if (cached.has_value())
        {
            return ServiceResult<FileRecord>::ok(*cached, "cache hit");
        }

        // 未命中，查库
        FileDao file_dao;
        auto file = file_dao.findByHash(normalized_hash);
        if (!file.has_value())
        {
            return ServiceResult<FileRecord>::fail("file not found");
        }

        // 写入缓存
        cache.put(file->file_hash, *file);

        return ServiceResult<FileRecord>::ok(*file, "query success");
    }

    ServiceListResult<FileRecord> FileService::listUserFiles(std::int64_t user_id, int limit, int offset)
    {
        if (user_id <= 0)
        {
            return ServiceListResult<FileRecord>::fail("user id is invalid");
        }

        if (limit <= 0 || limit > 100)
        {
            return ServiceListResult<FileRecord>::fail("limit must be between 1 and 100");
        }

        if (offset < 0)
        {
            return ServiceListResult<FileRecord>::fail("offset is invalid");
        }

        FileDao file_dao;
        return ServiceListResult<FileRecord>::ok(file_dao.listByUserId(user_id, limit, offset), "query success");
    }

    ServiceStatus FileService::updateStatus(std::int64_t file_id, int status)
    {
        if (file_id <= 0)
        {
            return ServiceStatus::fail("file id is invalid");
        }

        FileDao file_dao;
        if (!file_dao.updateStatus(file_id, status))
        {
            return ServiceStatus::fail("failed to update file status");
        }

        return ServiceStatus::ok("update status success");
    }

    ServiceStatus FileService::removeFile(std::int64_t file_id)
    {
        if (file_id <= 0)
        {
            return ServiceStatus::fail("file id is invalid");
        }

        // 从缓存中查找（保留信息用于清理缓存键）
        auto &id_cache = CacheManager::instance().fileCacheById();
        auto cached = id_cache.get(file_id);
        if (cached.has_value())
        {
            // 从哈希缓存也清除
            CacheManager::instance().fileCacheByHash().remove(cached->file_hash);
        }
        id_cache.remove(file_id);

        FileDao file_dao;
        if (!file_dao.removeById(file_id))
        {
            return ServiceStatus::fail("failed to remove file");
        }

        return ServiceStatus::ok("remove file success");
    }

} // namespace fileagent