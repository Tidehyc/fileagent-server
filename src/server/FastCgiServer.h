#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <fcgiapp.h>

#include "HttpContext.h"

namespace fileagent
{

    /**
     * @brief FastCGI 服务器封装
     *
     * 内部维护 FCGX_Accept 主循环，支持路径参数路由。
     * 用于替代 Drogon 的 HTTP 框架。
     */
    class FastCgiServer
    {
    public:
        using Handler = std::function<void(const HttpRequest &, HttpResponse &)>;

        ~FastCgiServer();

        /**
         * @brief 注册路由
         * @param method HTTP 方法（GET/POST/DELETE/PATCH）
         * @param pattern 路径模板（如 /api/files/{fileId}）
         * @param handler 处理函数
         */
        void registerHandler(const std::string &method,
                             const std::string &pattern,
                             Handler handler);

        // 便捷方法
        void get(const std::string &pattern, Handler h) { registerHandler("GET", pattern, std::move(h)); }
        void post(const std::string &pattern, Handler h) { registerHandler("POST", pattern, std::move(h)); }
        void del(const std::string &pattern, Handler h) { registerHandler("DELETE", pattern, std::move(h)); }
        void patch(const std::string &pattern, Handler h) { registerHandler("PATCH", pattern, std::move(h)); }

        /**
         * @brief 启动 FCGX_Accept 主循环
         */
        void run();

        /**
         * @brief 设置是否为监听套接字模式（否则从 stdin 接受 FastCGI 连接）
         */
        void setSocketPath(const std::string &path) { socket_path_ = path; }

    private:
        struct Route
        {
            std::string method;
            std::string pattern;
            std::vector<std::string> segments; // "/" 分割后的段，{name} 表示参数
            Handler handler;
        };

        std::vector<Route> routes_;
        std::string socket_path_;

        HttpRequest parseFcgiRequest(FCGX_Request &fcgiReq);
        void sendResponse(FCGX_Stream *out, const HttpResponse &resp);
        bool matchPath(const std::string &method, const std::string &path,
                       const Route &route,
                       std::unordered_map<std::string, std::string> &params) const;
    };

} // namespace fileagent
