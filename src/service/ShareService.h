#pragma once

#include <cstdint>
#include <string>

#include "ServiceTypes.h"

namespace fileagent
{

    class ShareService
    {
    public:
        /**
         * @brief 创建分享链接
         * @param user_id  当前用户
         * @param file_id  要分享的文件
         * @param expire_hours  过期小时数（0=永不过期）
         * @return ServiceResult<std::string> 成功返回 share_token
         */
        ServiceResult<std::string> createShare(std::int64_t user_id,
                                                std::int64_t file_id,
                                                int expire_hours = 0);
    };

} // namespace fileagent
