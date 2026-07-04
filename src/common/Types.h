#pragma once

#include <string>

namespace fileagent
{

    struct ApiResponse
    {
        int code = 0;
        std::string message = "success";
    };

} // namespace fileagent