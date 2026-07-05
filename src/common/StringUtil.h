#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace fileagent
{

    /**
     * @brief 去除字符串首尾空白字符
     */
    inline std::string trimCopy(std::string value)
    {
        auto is_space = [](unsigned char ch)
        {
            return std::isspace(ch) != 0;
        };

        value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char ch)
                                                { return !is_space(ch); }));
        value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char ch)
                                 { return !is_space(ch); })
                        .base(),
                    value.end());
        return value;
    }

} // namespace fileagent
