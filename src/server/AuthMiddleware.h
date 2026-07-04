#pragma once

#include <cstdint>
#include <string>

#include <drogon/HttpMiddleware.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

namespace fileagent
{

    /**
     * @brief 判断路径是否需要认证
     */
    bool isPublicPath(const std::string &path);

    /**
     * @brief 从请求中验证 token，返回 user_id
     * @return user_id（0=未认证）
     */
    std::int64_t authenticateRequest(const drogon::HttpRequestPtr &req);

    /**
     * @brief 创建 401 响应
     */
    drogon::HttpResponsePtr makeUnauthorizedResponse(const std::string &message);

} // namespace fileagent
