#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <drogon/drogon.h>
#include <trantor/utils/Utilities.h>

#include "common/Config.h"
#include "common/ConfigLoader.h"
#include "common/Hash.h"
#include "common/Logger.h"
#include "common/Types.h"
#include "dao/FileDao.h"
#include "dao/SessionDao.h"
#include "dao/UserDao.h"
#include "db/ConnectionPool.h"
#include "server/AuthMiddleware.h"
#include "service/AuthService.h"
#include "service/FileService.h"

using namespace drogon;
using namespace fileagent;

namespace
{

    // ─── 辅助函数 ──────────────────────────────────

    // 生成文件存储路径: uploads/{hash_prefix(2)}/{full_hash}
    std::string buildStoragePath(const std::string &file_hash)
    {
        if (file_hash.size() < 2)
        {
            return "uploads/" + file_hash;
        }
        return "uploads/" + file_hash.substr(0, 2) + "/" + file_hash;
    }

    // 确保目录存在
    bool ensureDirectory(const std::string &path)
    {
        std::filesystem::path dir(path);
        if (!std::filesystem::exists(dir))
        {
            return std::filesystem::create_directories(dir);
        }
        return true;
    }

    // 从请求中验证 token 并提取 user_id（0=未认证）
    std::int64_t getUserIdFromRequest(const HttpRequestPtr &req)
    {
        return authenticateRequest(req);
    }

    // 创建 JSON 响应
    HttpResponsePtr makeJsonResponse(int code, const std::string &message)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setBody("{\"code\":" + std::to_string(code) + ",\"message\":\"" + message + "\"}");
        return resp;
    }

    HttpResponsePtr makeJsonResponse(int code, const std::string &message, const std::string &data_field, const std::string &data_value)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setBody("{\"code\":" + std::to_string(code) + ",\"message\":\"" + message + "\",\"" + data_field + "\":\"" + data_value + "\"}");
        return resp;
    }

    // ─── 路由处理器 ──────────────────────────────────

    // GET /health
    void handleHealth(const HttpRequestPtr &,
                      std::function<void(const HttpResponsePtr &)> &&callback)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody("ok");
        resp->setContentTypeCode(CT_TEXT_PLAIN);
        callback(resp);
    }

    // POST /api/user/register
    void handleRegister(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback)
    {
        auto json = req->getJsonObject();
        if (!json)
        {
            callback(makeJsonResponse(400, "request body must be JSON"));
            return;
        }

        RegisterRequest reg_req{
            (*json)["username"].asString(),
            (*json)["password"].asString(),
            (*json)["email"].asString(),
        };

        AuthService auth_service;
        auto result = auth_service.registerUser(reg_req);
        if (!result.success)
        {
            callback(makeJsonResponse(400, result.message));
            return;
        }

        callback(makeJsonResponse(0, "register success"));
    }

    // POST /api/user/login
    void handleLogin(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback)
    {
        auto json = req->getJsonObject();
        if (!json)
        {
            callback(makeJsonResponse(400, "request body must be JSON"));
            return;
        }

        LoginRequest login_req{
            (*json)["username"].asString(),
            (*json)["password"].asString(),
        };

        // 验证用户
        AuthService auth_service;
        auto result = auth_service.login(login_req);
        if (!result.success)
        {
            callback(makeJsonResponse(401, result.message));
            return;
        }

        // 生成 session token（64 字符 hex）
        std::string token = PasswordHasher::generateToken(32);

        // 过期时间：7 天
        auto now = std::chrono::system_clock::now();
        auto expires = now + std::chrono::hours(24 * 7);
        auto expires_t = std::chrono::system_clock::to_time_t(expires);
        std::string expires_str = std::ctime(&expires_t);
        // ctime 包含换行符，转为 YYYY-MM-DD HH:MM:SS 格式
        char expires_buf[32];
        std::strftime(expires_buf, sizeof(expires_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&expires_t));

        // 保存 session
        SessionDao session_dao;
        if (!session_dao.createSession(result.data->id, token, expires_buf))
        {
            logError("handleLogin: failed to create session");
            callback(makeJsonResponse(500, "failed to create session"));
            return;
        }

        // 返回 token
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setBody("{\"code\":0,\"message\":\"login success\",\"token\":\"" + token + "\",\"user_id\":" + std::to_string(result.data->id) + "}");
        callback(resp);
    }

    // POST /api/user/logout
    void handleLogout(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback)
    {
        std::int64_t user_id = getUserIdFromRequest(req);
        if (user_id == 0)
        {
            callback(makeJsonResponse(401, "unauthorized"));
            return;
        }

        const auto &auth_header = req->getHeader("Authorization");
        std::string token = auth_header.substr(7); // 跳过 "Bearer "

        SessionDao session_dao;
        session_dao.deleteByToken(token);

        callback(makeJsonResponse(0, "logout success"));
    }

    // POST /api/files/upload
    void handleUpload(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback)
    {
        std::int64_t user_id = getUserIdFromRequest(req);
        if (user_id == 0)
        {
            callback(makeJsonResponse(401, "unauthorized"));
            return;
        }

        // 解析 multipart 请求
        MultiPartParser parser;
        if (parser.parse(req) != 0 || parser.getFiles().empty())
        {
            callback(makeJsonResponse(400, "no file uploaded"));
            return;
        }

        auto &upload_file = parser.getFiles()[0];
        std::string filename = upload_file.getFileName();
        std::string file_content(upload_file.fileContent());

        if (file_content.empty())
        {
            callback(makeJsonResponse(400, "empty file"));
            return;
        }

        // 计算 SHA-256 哈希
        auto hash_bytes = trantor::utils::sha256(file_content);
        std::string file_hash = trantor::utils::toHexString(hash_bytes);

        // 检查文件是否已存在（去重）
        FileService file_service;
        auto existing = file_service.getFileByHash(file_hash);
        if (existing.success)
        {
            // 已存在相同文件，直接返回已有记录
            auto &file = *existing.data;
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k200OK);
            resp->setContentTypeCode(CT_APPLICATION_JSON);
            resp->setBody("{\"code\":0,\"message\":\"file already exists\",\"file_id\":" + std::to_string(file.id) + ",\"hash\":\"" + file.file_hash + "\"}");
            callback(resp);
            return;
        }

        // 构建存储路径
        std::string storage_path = buildStoragePath(file_hash);
        std::string dir_part = storage_path.substr(0, storage_path.find_last_of('/'));

        // 确保目录存在
        if (!ensureDirectory(dir_part))
        {
            logError("handleUpload: failed to create directory: " + dir_part);
            callback(makeJsonResponse(500, "failed to create storage directory"));
            return;
        }

        // 写入磁盘
        {
            std::ofstream outfile(storage_path, std::ios::binary);
            if (!outfile)
            {
                logError("handleUpload: failed to write file: " + storage_path);
                callback(makeJsonResponse(500, "failed to write file"));
                return;
            }
            outfile.write(file_content.data(), static_cast<std::streamsize>(file_content.size()));
        }

        // 入库
        FileRecord record;
        record.user_id = user_id;
        record.filename = filename;
        record.file_hash = file_hash;
        record.file_size = static_cast<std::int64_t>(file_content.size());
        record.storage_path = storage_path;
        record.status = 1;

        auto create_result = file_service.createFile(record);
        if (!create_result.success)
        {
            logError("handleUpload: failed to create file record: " + create_result.message);
            // 清理已写入的文件
            std::filesystem::remove(storage_path);
            callback(makeJsonResponse(500, "failed to create file record"));
            return;
        }

        logInfo("File uploaded: user=" + std::to_string(user_id) +
                ", filename=" + filename +
                ", size=" + std::to_string(record.file_size) +
                ", hash=" + file_hash);

        auto &created = *create_result.data;
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setBody("{\"code\":0,\"message\":\"upload success\",\"file_id\":" + std::to_string(created.id) + ",\"hash\":\"" + file_hash + "\"}");
        callback(resp);
    }

    // GET /api/files
    void handleListFiles(const HttpRequestPtr &req,
                         std::function<void(const HttpResponsePtr &)> &&callback)
    {
        std::int64_t user_id = getUserIdFromRequest(req);
        if (user_id == 0)
        {
            callback(makeJsonResponse(401, "unauthorized"));
            return;
        }

        // 解析分页参数
        int limit = 20;
        int offset = 0;
        auto limit_param = req->getParameter("limit");
        auto offset_param = req->getParameter("offset");
        if (!limit_param.empty())
            limit = std::stoi(limit_param);
        if (!offset_param.empty())
            offset = std::stoi(offset_param);

        FileService file_service;
        auto result = file_service.listUserFiles(user_id, limit, offset);
        if (!result.success)
        {
            callback(makeJsonResponse(400, result.message));
            return;
        }

        // 构建 JSON 数组
        Json::Value files_json(Json::arrayValue);
        for (const auto &f : result.data)
        {
            Json::Value item;
            item["id"] = f.id;
            item["filename"] = f.filename;
            item["file_hash"] = f.file_hash;
            item["file_size"] = f.file_size;
            item["status"] = f.status;
            item["created_at"] = f.created_at;
            files_json.append(item);
        }

        Json::Value resp_json;
        resp_json["code"] = 0;
        resp_json["message"] = "query success";
        resp_json["files"] = files_json;
        resp_json["total"] = static_cast<int>(result.data.size());

        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setBody(Json::writeString(Json::StreamWriterBuilder(), resp_json));
        callback(resp);
    }

    // GET /api/files/{fileId}
    void handleDownload(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback,
                        const std::string &fileId_str)
    {
        std::int64_t user_id = getUserIdFromRequest(req);
        if (user_id == 0)
        {
            callback(makeJsonResponse(401, "unauthorized"));
            return;
        }

        std::int64_t file_id = std::stoll(fileId_str);
        FileService file_service;
        auto result = file_service.getFileById(file_id);
        if (!result.success)
        {
            callback(makeJsonResponse(404, "file not found"));
            return;
        }

        auto &file = *result.data;

        // 验证文件属于当前用户
        if (file.user_id != user_id)
        {
            callback(makeJsonResponse(403, "access denied"));
            return;
        }

        // 检查文件是否在磁盘上存在
        if (!std::filesystem::exists(file.storage_path))
        {
            logError("handleDownload: file not found on disk: " + file.storage_path);
            callback(makeJsonResponse(404, "file not found on disk"));
            return;
        }

        // 流式返回文件
        auto resp = HttpResponse::newFileResponse(file.storage_path);
        resp->addHeader("Content-Disposition", "attachment; filename=\"" + file.filename + "\"");
        callback(resp);
    }

    // DELETE /api/files/{fileId}
    void handleDelete(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback,
                      const std::string &fileId_str)
    {
        std::int64_t user_id = getUserIdFromRequest(req);
        if (user_id == 0)
        {
            callback(makeJsonResponse(401, "unauthorized"));
            return;
        }

        std::int64_t file_id = std::stoll(fileId_str);

        // 验证文件属于当前用户
        FileService file_service;
        auto file_result = file_service.getFileById(file_id);
        if (!file_result.success)
        {
            callback(makeJsonResponse(404, "file not found"));
            return;
        }

        if (file_result.data->user_id != user_id)
        {
            callback(makeJsonResponse(403, "access denied"));
            return;
        }

        // 删除磁盘文件
        const auto &storage_path = file_result.data->storage_path;
        if (!storage_path.empty() && std::filesystem::exists(storage_path))
        {
            std::error_code ec;
            std::filesystem::remove(storage_path, ec);
            if (ec)
            {
                logWarn("handleDelete: failed to delete file on disk: " + storage_path + " - " + ec.message());
            }
        }

        // 删除数据库记录
        auto status_result = file_service.removeFile(file_id);
        if (!status_result.success)
        {
            callback(makeJsonResponse(500, status_result.message));
            return;
        }

        logInfo("File deleted: user=" + std::to_string(user_id) + ", file_id=" + std::to_string(file_id));
        callback(makeJsonResponse(0, "delete success"));
    }

} // anonymous namespace

int main(int argc, char *argv[])
{
    // 确定配置文件路径
    std::string config_path = "config.yaml";
    if (argc > 1)
    {
        config_path = argv[1];
    }

    // 加载配置
    AppConfig app_config;
    if (!std::filesystem::exists(config_path))
    {
        logError("Configuration file not found: " + config_path);
        return 1;
    }

    if (!app_config.loadFromFile(config_path))
    {
        logError("Failed to load configuration from: " + config_path);
        return 1;
    }

    // 验证配置
    if (!app_config.validate())
    {
        logError("Configuration validation failed");
        return 1;
    }

    if (!ConnectionPool::instance().init(app_config.database))
    {
        logError("Failed to initialize MySQL connection pool");
        return 1;
    }

    logInfo("Configuration loaded and validated");
    logDebug(app_config.toString());
    logInfo("MySQL pool initialized: available=" + std::to_string(ConnectionPool::instance().available()) +
            ", active=" + std::to_string(ConnectionPool::instance().active()));

    // ─── HTTP 服务 ─────────────────────────────────

    // 确保上传目录存在
    ensureDirectory("uploads");
    std::cout << R"(
    ┌─────────────────────────────────────────┐
    │        FileAgent Server                 │
    │   C++20  AI 智能文件管理服务             │
    │   Drogon (epoll) + MySQL + llama.cpp    │
    └─────────────────────────────────────────┘
    )" << std::endl;

    logInfo("Starting server on " + app_config.server.host + ":" + std::to_string(app_config.server.port));
    logInfo("Database: " + app_config.database.host + ":" + std::to_string(app_config.database.port) +
            "/" + app_config.database.database);
    logInfo("Thread count: " + std::to_string(app_config.server.threads));

    // 注册全局认证中间件
    // app().registerMiddleware(std::make_shared<AuthMiddleware>());

    // ─── 注册全部路由 ──────────────────────────────

    // 公共路由
    app().registerHandler("/health", &handleHealth, {Get});
    app().registerHandler("/api/user/register", &handleRegister, {Post});
    app().registerHandler("/api/user/login", &handleLogin, {Post});

    // 受保护路由
    app().registerHandler("/api/user/logout", &handleLogout, {Post});
    app().registerHandler("/api/files/upload", &handleUpload, {Post});
    app().registerHandler("/api/files", &handleListFiles, {Get});
    app().registerHandler("/api/files/{fileId}", &handleDownload, {Get});
    app().registerHandler("/api/files/{fileId}", &handleDelete, {Delete});

    // 启动
    app().addListener(app_config.server.host, app_config.server.port)
        .setThreadNum(static_cast<unsigned int>(app_config.server.threads))
        .run();

    return 0;
}
