#pragma once

#include <cstdint>

#include "../dao/Models.h"
#include "ServiceTypes.h"

namespace fileagent
{

    class FileService
    {
    public:
        ServiceResult<FileRecord> createFile(const FileRecord &record);
        ServiceResult<FileRecord> getFileById(std::int64_t file_id);
        ServiceResult<FileRecord> getFileByHash(const std::string &file_hash);
        ServiceListResult<FileRecord> listUserFiles(std::int64_t user_id, int limit = 20, int offset = 0);
        ServiceStatus updateStatus(std::int64_t file_id, int status);
        ServiceStatus removeFile(std::int64_t file_id);

    private:
        bool validateRecord(const FileRecord &record, std::string &message) const;
    };

} // namespace fileagent