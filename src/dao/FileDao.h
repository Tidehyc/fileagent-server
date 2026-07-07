#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Models.h"

namespace fileagent
{

    class FileDao
    {
    public:
        bool createFile(const FileRecord &record);

        std::optional<FileRecord> findById(std::int64_t id);
        std::optional<FileRecord> findByHash(const std::string &file_hash);
        std::vector<FileRecord> listByUserId(std::int64_t user_id, int limit = 20, int offset = 0);

        bool updateStatus(std::int64_t id, int status);
        bool removeById(std::int64_t id);

        /// 更新文件摘要和标签（AI 生成）
        bool updateFileMeta(std::int64_t id,
                            const std::string &summary,
                            const std::string &tags);
    };

} // namespace fileagent