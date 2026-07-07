#include "FastCgiServer.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

#include "../common/Logger.h"

namespace fileagent
{

    // ─── HttpRequest 成员实现 ────────────────────────

    std::string HttpRequest::getHeader(const std::string &name) const
    {
        auto it = headers.find(name);
        return it != headers.end() ? it->second : "";
    }

    std::string HttpRequest::getQuery(const std::string &name, const std::string &def) const
    {
        auto it = queries.find(name);
        return it != queries.end() ? it->second : def;
    }

    std::string HttpRequest::getParam(const std::string &name, const std::string &def) const
    {
        auto it = params.find(name);
        return it != params.end() ? it->second : def;
    }

    // ─── multipart/form-data 解析 ─────────────────────

    std::vector<MultipartFile> parseMultipartFormData(const std::string &body,
                                                       const std::string &boundary)
    {
        std::vector<MultipartFile> files;
        std::string delim = "--" + boundary;
        std::string end_delim = delim + "--";

        size_t pos = 0;
        while (true)
        {
            // 查找下一个分隔符
            size_t start = body.find(delim, pos);
            if (start == std::string::npos)
                break;

            // 判断是否是结束分隔符
            if (body.compare(start, end_delim.size(), end_delim) == 0)
                break;

            // 跳过分隔符行
            start = body.find("\r\n", start);
            if (start == std::string::npos)
                break;
            start += 2; // 跳过 \r\n

            // 查找下一个分隔符
            size_t end = body.find("\r\n" + delim, start);
            if (end == std::string::npos)
            {
                // 可能是最后一个部分以结束分隔符结尾
                end = body.find(end_delim, start);
                if (end == std::string::npos)
                    end = body.size();
            }

            // 取出这一部分
            std::string part = body.substr(start, end - start);

            // 解析头部和内容
            size_t header_end = part.find("\r\n\r\n");
            if (header_end == std::string::npos)
            {
                pos = end;
                continue;
            }

            std::string part_headers = part.substr(0, header_end);
            std::string part_content = part.substr(header_end + 4);

            // 去除尾部 \r\n
            if (part_content.size() >= 2 && part_content.substr(part_content.size() - 2) == "\r\n")
                part_content.resize(part_content.size() - 2);

            // 提取 Content-Disposition 中的 filename
            MultipartFile file;
            size_t fn_pos = part_headers.find("filename=\"");
            if (fn_pos != std::string::npos)
            {
                fn_pos += 10;
                size_t fn_end = part_headers.find('"', fn_pos);
                if (fn_end != std::string::npos)
                    file.filename = part_headers.substr(fn_pos, fn_end - fn_pos);
            }

            // 提取 Content-Type
            size_t ct_pos = part_headers.find("Content-Type:");
            if (ct_pos != std::string::npos)
            {
                ct_pos += 13;
                while (ct_pos < part_headers.size() && part_headers[ct_pos] == ' ')
                    ct_pos++;
                size_t ct_end = part_headers.find("\r\n", ct_pos);
                if (ct_end != std::string::npos)
                    file.content_type = part_headers.substr(ct_pos, ct_end - ct_pos);
            }

            file.content = std::move(part_content);
            files.push_back(std::move(file));

            pos = end;
        }

        return files;
    }

    // ─── FastCgiServer ───────────────────────────────

    FastCgiServer::~FastCgiServer() = default;

    void FastCgiServer::registerHandler(const std::string &method,
                                        const std::string &pattern,
                                        Handler handler)
    {
        // 将 pattern 按 "/" 分割为 segments
        std::vector<std::string> segments;
        std::stringstream ss(pattern);
        std::string seg;
        while (std::getline(ss, seg, '/'))
        {
            if (!seg.empty())
                segments.push_back(seg);
        }

        routes_.push_back({method, pattern, std::move(segments), std::move(handler)});
        logInfo("Route registered: " + method + " " + pattern);
    }

    bool FastCgiServer::matchPath(const std::string &method,
                                   const std::string &path,
                                   const Route &route,
                                   std::unordered_map<std::string, std::string> &params) const
    {
        // 方法必须匹配
        if (route.method != method)
            return false;

        // 分割请求路径
        std::vector<std::string> path_segs;
        std::stringstream ss(path);
        std::string seg;
        while (std::getline(ss, seg, '/'))
        {
            if (!seg.empty())
                path_segs.push_back(seg);
        }

        // 段数必须相同
        if (route.segments.size() != path_segs.size())
            return false;

        // 逐段匹配
        params.clear();
        for (size_t i = 0; i < route.segments.size(); i++)
        {
            const auto &pattern_seg = route.segments[i];
            const auto &path_seg = path_segs[i];

            if (pattern_seg.size() > 2 && pattern_seg.front() == '{' && pattern_seg.back() == '}')
            {
                // 参数段，提取参数名
                std::string param_name = pattern_seg.substr(1, pattern_seg.size() - 2);
                params[param_name] = path_seg;
            }
            else
            {
                // 静态段，必须精确匹配
                if (pattern_seg != path_seg)
                    return false;
            }
        }

        return true;
    }

    HttpRequest FastCgiServer::parseFcgiRequest(FCGX_Request &fcgiReq)
    {
        HttpRequest req;

        // 从环境变量提取基本信息
        auto getEnv = [&](const char *name) -> std::string {
            const char *val = FCGX_GetParam(name, fcgiReq.envp);
            return val ? std::string(val) : "";
        };

        req.method = getEnv("REQUEST_METHOD");
        req.path = getEnv("REQUEST_URI");

        // 去除查询字符串部分
        std::string full_path = req.path;
        size_t qmark = full_path.find('?');
        if (qmark != std::string::npos)
        {
            req.path = full_path.substr(0, qmark);

            // 解析查询参数
            std::string query_str = full_path.substr(qmark + 1);
            std::stringstream qss(query_str);
            std::string pair;
            while (std::getline(qss, pair, '&'))
            {
                size_t eq = pair.find('=');
                if (eq != std::string::npos)
                {
                    std::string key = pair.substr(0, eq);
                    std::string value = pair.substr(eq + 1);
                    // URL 解码（简化版，将 %XX 转义）
                    // 实际使用中应进行完整解码
                    req.queries[key] = value;
                }
                else if (!pair.empty())
                {
                    req.queries[pair] = "";
                }
            }
        }

        // 请求头
        // HTTP 头在 FastCGI 中以 HTTP_ 前缀的环境变量传递
        {
            // 直接从环境变量获取常见请求头
            std::string auth = getEnv("HTTP_AUTHORIZATION");
            if (!auth.empty())
                req.headers["Authorization"] = auth;

            std::string ct = getEnv("CONTENT_TYPE");
            if (!ct.empty())
                req.headers["Content-Type"] = ct;

            std::string cl = getEnv("CONTENT_LENGTH");
            if (!cl.empty())
                req.headers["Content-Length"] = cl;

            std::string ua = getEnv("HTTP_USER_AGENT");
            if (!ua.empty())
                req.headers["User-Agent"] = ua;

            std::string cookie = getEnv("HTTP_COOKIE");
            if (!cookie.empty())
                req.headers["Cookie"] = cookie;
        }

        // 读取请求体（POST/PATCH/DELETE）
        std::string content_length_str = getEnv("CONTENT_LENGTH");
        if (!content_length_str.empty())
        {
            int content_length = std::stoi(content_length_str);
            if (content_length > 0)
            {
                req.body.resize(content_length);
                int bytes_read = FCGX_GetStr(req.body.data(), content_length, fcgiReq.in);
                if (bytes_read > 0)
                    req.body.resize(bytes_read);
                else
                    req.body.clear();
            }
        }
        // 对于 multipart 请求，也可能有分块传输但仍由 CONTENT_LENGTH 指示
        // 如果没有 CONTENT_LENGTH 但有输入，尝试读取直到 EOF（很少见）

        return req;
    }

    void FastCgiServer::sendResponse(FCGX_Stream *out, const HttpResponse &resp)
    {
        // 响应行
        const char *status_text = "OK";
        if (resp.statusCode == 400)
            status_text = "Bad Request";
        else if (resp.statusCode == 401)
            status_text = "Unauthorized";
        else if (resp.statusCode == 403)
            status_text = "Forbidden";
        else if (resp.statusCode == 404)
            status_text = "Not Found";
        else if (resp.statusCode == 500)
            status_text = "Internal Server Error";

        FCGX_FPrintF(out, "Status: %d %s\r\n", resp.statusCode, status_text);
        FCGX_FPrintF(out, "Content-Type: %s\r\n", resp.contentType.c_str());
        FCGX_FPrintF(out, "Content-Length: %d\r\n", static_cast<int>(resp.body.size()));

        for (const auto &[key, value] : resp.headers)
        {
            FCGX_FPrintF(out, "%s: %s\r\n", key.c_str(), value.c_str());
        }

        FCGX_FPrintF(out, "\r\n");
        FCGX_PutStr(resp.body.data(), resp.body.size(), out);
    }

    void FastCgiServer::run()
    {
        // 初始化 FCGI
        FCGX_Init();

        int listen_sock = -1;
        if (!socket_path_.empty())
        {
            // 监听模式：自己打开 Unix Socket
            listen_sock = FCGX_OpenSocket(socket_path_.c_str(), 20);
            if (listen_sock < 0)
            {
                logError("FastCgiServer: failed to open socket: " + socket_path_);
                return;
            }
            logInfo("FastCgiServer: listening on " + socket_path_);
        }

        logInfo("");
        logInfo("  ╔══════════════════════════════════════════╗");
        logInfo("  ║         FileAgent Server                 ║");
        logInfo("  ║   C++20 AI 智能文件管理服务              ║");
        logInfo("  ║   FastCGI + Nginx + MySQL + Redis + LLM  ║");
        logInfo("  ╚══════════════════════════════════════════╝");
        logInfo("");

        logInfo("FastCgiServer: " + std::to_string(routes_.size()) + " routes registered, accepting requests...");

        FCGX_Request request;
        FCGX_InitRequest(&request, listen_sock, 0);

        while (FCGX_Accept_r(&request) == 0)
        {
            // 解析请求
            HttpRequest req = parseFcgiRequest(request);
            HttpResponse resp;

            // 路由匹配
            bool matched = false;
            for (const auto &route : routes_)
            {
                std::unordered_map<std::string, std::string> params;
                if (matchPath(req.method, req.path, route, params))
                {
                    req.params = std::move(params);
                    try
                    {
                        route.handler(req, resp);
                    }
                    catch (const std::exception &e)
                    {
                        logError("Handler exception for " + req.method + " " + req.path + ": " + e.what());
                        resp = HttpResponse();
                        resp.statusCode = 500;
                        resp.contentType = "application/json";
                        resp.body = "{\"code\":500,\"message\":\"internal server error\"}";
                    }
                    matched = true;
                    break;
                }
            }

            if (!matched)
            {
                resp.statusCode = 404;
                resp.contentType = "application/json";
                resp.body = "{\"code\":404,\"message\":\"route not found: " + req.method + " " + req.path + "\"}";
                logWarn("No route matched: " + req.method + " " + req.path);
            }

            // 发送响应
            sendResponse(request.out, resp);

            // 完成请求
            FCGX_Finish_r(&request);
        }

        logInfo("FastCgiServer: exiting accept loop");

        if (listen_sock >= 0)
        {
            close(listen_sock);
            if (!socket_path_.empty())
                unlink(socket_path_.c_str());
        }
    }

} // namespace fileagent
