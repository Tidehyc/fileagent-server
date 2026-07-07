#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace fileagent
{

    /**
     * @brief HTTP 请求结构体 — 替代 Drogon HttpRequest
     */
    struct HttpRequest
    {
        std::string method;                                  // GET / POST / DELETE / PATCH
        std::string path;                                    // 请求路径
        std::unordered_map<std::string, std::string> params; // 路径参数 {fileId → "42"}
        std::unordered_map<std::string, std::string> queries; // 查询参数 {limit → "20"}
        std::unordered_map<std::string, std::string> headers; // 请求头
        std::string body;                                    // 请求体

        std::string getHeader(const std::string &name) const;
        std::string getQuery(const std::string &name, const std::string &def = "") const;
        std::string getParam(const std::string &name, const std::string &def = "") const;
    };

    /**
     * @brief HTTP 响应结构体 — 替代 Drogon HttpResponse
     */
    struct HttpResponse
    {
        int statusCode = 200;
        std::string contentType = "text/plain";
        std::unordered_map<std::string, std::string> headers;
        std::string body;

        void setStatusCode(int code) { statusCode = code; }
        void setContentType(const std::string &ct) { contentType = ct; }
        void setBody(const std::string &b) { body = b; }
        void setHeader(const std::string &key, const std::string &value) { headers[key] = value; }
    };

    /**
     * @brief multipart/form-data 中的文件部分
     */
    struct MultipartFile
    {
        std::string filename;
        std::string content_type;
        std::string content;
    };

    /**
     * @brief 解析 multipart/form-data
     * @param body 请求体
     * @param boundary 分隔边界（不含 "--"）
     * @return 解析出的文件列表
     */
    std::vector<MultipartFile> parseMultipartFormData(const std::string &body,
                                                       const std::string &boundary);

} // namespace fileagent
