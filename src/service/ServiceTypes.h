#pragma once

#include <optional>
#include <string>
#include <vector>

namespace fileagent
{

    struct ServiceStatus
    {
        bool success{false};
        std::string message;

        static ServiceStatus ok(std::string message = "success")
        {
            return {true, std::move(message)};
        }

        static ServiceStatus fail(std::string message)
        {
            return {false, std::move(message)};
        }
    };

    template <typename T>
    struct ServiceResult
    {
        bool success{false};
        std::string message;
        std::optional<T> data;

        static ServiceResult ok(T value, std::string message = "success")
        {
            ServiceResult result;
            result.success = true;
            result.message = std::move(message);
            result.data = std::move(value);
            return result;
        }

        static ServiceResult fail(std::string message)
        {
            ServiceResult result;
            result.success = false;
            result.message = std::move(message);
            return result;
        }
    };

    template <typename T>
    struct ServiceListResult
    {
        bool success{false};
        std::string message;
        std::vector<T> data;

        static ServiceListResult ok(std::vector<T> value, std::string message = "success")
        {
            ServiceListResult result;
            result.success = true;
            result.message = std::move(message);
            result.data = std::move(value);
            return result;
        }

        static ServiceListResult fail(std::string message)
        {
            ServiceListResult result;
            result.success = false;
            result.message = std::move(message);
            return result;
        }
    };

} // namespace fileagent