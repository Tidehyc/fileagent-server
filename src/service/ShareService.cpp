#include "ShareService.h"

#include <chrono>
#include <ctime>

#include "../common/Hash.h"
#include "../common/Logger.h"
#include "../dao/FileDao.h"
#include "../dao/ShareDao.h"

namespace fileagent
{

    ServiceResult<std::string> ShareService::createShare(std::int64_t user_id,
                                                          std::int64_t file_id,
                                                          int expire_hours)
    {
        if (file_id <= 0)
        {
            return ServiceResult<std::string>::fail("invalid file id");
        }

        // 验证文件存在且属于当前用户
        FileDao file_dao;
        auto file = file_dao.findById(file_id);
        if (!file.has_value())
        {
            return ServiceResult<std::string>::fail("file not found");
        }
        if (file->user_id != user_id)
        {
            return ServiceResult<std::string>::fail("file does not belong to user");
        }

        // 生成唯一 token
        std::string token = PasswordHasher::generateToken(16);

        // 计算过期时间
        std::string expires_at;
        if (expire_hours > 0)
        {
            auto now = std::chrono::system_clock::now();
            auto expires = now + std::chrono::hours(expire_hours);
            auto expires_t = std::chrono::system_clock::to_time_t(expires);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&expires_t));
            expires_at = buf;
        }

        // 入库
        ShareDao share_dao;
        if (!share_dao.createShare(file_id, user_id, token, expires_at))
        {
            logError("ShareService::createShare: failed to insert");
            return ServiceResult<std::string>::fail("failed to create share");
        }

        logInfo("Share created: token=" + token + " file_id=" + std::to_string(file_id) +
                " expire=" + (expire_hours > 0 ? std::to_string(expire_hours) + "h" : "never"));

        return ServiceResult<std::string>::ok(token, "share created");
    }

} // namespace fileagent
