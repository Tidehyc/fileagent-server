#include "AuthMiddleware.h"

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>

#include "../common/Logger.h"
#include "../dao/SessionDao.h"

using namespace drogon;

namespace fileagent
{

    // ─── 公共路径 ──────────────────────────────────
    bool isPublicPath(const std::string &path)
    {
        return path == "/health" ||
               path == "/api/user/register" ||
               path == "/api/user/login";
    }

    // ─── 从请求提取并验证 token，返回 user_id（0=未认证） ──
    std::int64_t authenticateRequest(const HttpRequestPtr &req)
    {
        const auto &auth_header = req->getHeader("Authorization");
        if (auth_header.empty())
        {
            return 0;
        }

        const std::string prefix = "Bearer ";
        if (auth_header.size() <= prefix.size() ||
            auth_header.substr(0, prefix.size()) != prefix)
        {
            return 0;
        }

        std::string token = auth_header.substr(prefix.size());
        if (token.empty())
        {
            return 0;
        }

        SessionDao session_dao;
        auto session = session_dao.findByToken(token);
        if (!session.has_value())
        {
            return 0;
        }

        return session->user_id;
    }

    // ─── 创建 401 JSON 响应 ─────────────────────
    HttpResponsePtr makeUnauthorizedResponse(const std::string &message)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setBody("{\"code\":401,\"message\":\"" + message + "\"}");
        return resp;
    }

} // namespace fileagent
