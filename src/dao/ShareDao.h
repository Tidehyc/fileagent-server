#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Models.h"

namespace fileagent
{

    class ShareDao
    {
    public:
        bool createShare(std::int64_t file_id,
                         std::int64_t user_id,
                         const std::string &token,
                         const std::string &expiresAt);

        std::optional<ShareRecord> findByToken(const std::string &token);

        bool deleteByToken(const std::string &token);

        std::vector<ShareRecord> listByUserId(std::int64_t user_id);
    };

} // namespace fileagent
