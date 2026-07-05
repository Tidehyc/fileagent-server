#pragma once

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
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

    /**
     * @brief JSON 字符串转义
     *
     * 将字符串中的特殊字符转义为 JSON 兼容的格式：
     *   " → \"    \ → \\    \n → \\n    \r → \\r
     *   \t → \\t  \b → \\b  \f → \\f  控制字符 → \\uXXXX
     */
    inline std::string jsonEscape(const std::string &raw)
    {
        std::ostringstream oss;
        for (unsigned char ch : raw)
        {
            switch (ch)
            {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b";  break;
            case '\f': oss << "\\f";  break;
            case '\n': oss << "\\n";  break;
            case '\r': oss << "\\r";  break;
            case '\t': oss << "\\t";  break;
            default:
                if (ch < 0x20)
                {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(ch);
                }
                else
                {
                    oss << ch;
                }
                break;
            }
        }
        return oss.str();
    }

} // namespace fileagent
