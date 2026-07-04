#pragma once

#include <iostream>
#include <string_view>

namespace fileagent
{

    inline void logInfo(std::string_view message)
    {
        std::cout << "[INFO] " << message << std::endl;
    }

    inline void logError(std::string_view message)
    {
        std::cerr << "[ERROR] " << message << std::endl;
    }

    inline void logWarn(std::string_view message)
    {
        std::cout << "[WARN] " << message << std::endl;
    }

    inline void logDebug(std::string_view message)
    {
        std::cout << "[DEBUG] " << message << std::endl;
    }

} // namespace fileagent