#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "agent/AgentEngine.h"
#include "common/CacheManager.h"
#include "common/Config.h"
#include "common/FastDfsClient.h"
#include "common/Hash.h"
#include "common/Logger.h"
#include "common/Sha256.h"
#include "common/RateLimiter.h"
#include "common/TextExtractor.h"
#include "dao/FileDao.h"
#include "dao/Models.h"
#include "dao/SessionDao.h"
#include "dao/ShareDao.h"
#include "dao/UserDao.h"
#include "db/ConnectionPool.h"
#include "db/MysqlConn.h"
#include "server/FastCgiServer.h"
#include "service/AuthService.h"
#include "service/ChunkUploadService.h"
#include "service/FileService.h"
#include "service/LlmService.h"
#include "service/ShareService.h"
#include "vector/TextChunker.h"
#include "vector/VectorStore.h"

using json = nlohmann::json;
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

  // ─── 认证与验证工具 ──────────────────────────────

  // 从请求中验证 token 并提取 user_id（0=未认证）
  std::int64_t authenticateRequest(const HttpRequest &req)
  {
    const auto &auth_header = req.getHeader("Authorization");
    if (auth_header.empty())
      return 0;

    const std::string prefix = "Bearer ";
    if (auth_header.size() <= prefix.size() ||
        auth_header.substr(0, prefix.size()) != prefix)
      return 0;

    std::string token = auth_header.substr(prefix.size());
    if (token.empty())
      return 0;

    SessionDao session_dao;
    auto session = session_dao.findByToken(token);
    if (!session.has_value())
      return 0;

    return session->user_id;
  }

  // 创建 JSON 响应（nlohmann/json 自动转义字符串）
  void makeJsonResponse(HttpResponse &resp, int code, const std::string &message)
  {
    resp.statusCode = 200;
    resp.contentType = "application/json";
    json j;
    j["code"] = code;
    j["message"] = message;
    resp.body = j.dump();
  }

  // 认证检查：失败时自动发送 401 并返回 0
  std::int64_t requireAuth(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = authenticateRequest(req);
    if (user_id == 0)
    {
      makeJsonResponse(resp, 401, "unauthorized");
    }
    return user_id;
  }

  // JSON 请求体验证：失败时自动发送 400 并返回 nullptr
  const json *requireJsonBody(const HttpRequest &req, HttpResponse &resp)
  {
    try
    {
      static thread_local json cached_json;
      cached_json = json::parse(req.body);
      return &cached_json;
    }
    catch (const std::exception &)
    {
      makeJsonResponse(resp, 400, "request body must be JSON");
      return nullptr;
    }
  }

  // ─── LLM 服务全局实例 ─────────────────────────────
  static std::unique_ptr<LlmService> g_llm_service;

  // ─── 向量存储全局实例 ─────────────────────────────
  static VectorStore g_vector_store;

  // ─── Agent 引擎全局实例 ─────────────────────────────
  static AgentEngine g_agent_engine;
  static RateLimiter g_rate_limiter(10, 60); // 每个 IP 每 60 秒最多 10 次

  // ─── FastDFS 客户端全局实例 ─────────────────────────
  static std::unique_ptr<FastDfsClient> g_fastdfs;
  static std::string g_fastdfs_data_path; // Nginx X-Accel-Redirect 的数据目录

  // ─── 存储辅助函数（支持 Local / FastDFS 双后端） ────

  // 写入文件 / 上传到 FastDFS
  bool storeFile(const std::string &content,
                 const std::string &filename,
                 const std::string &file_hash,
                 std::string &storage_path)
  {
    if (g_fastdfs && g_fastdfs->initialized())
    {
      // 新文件走 FastDFS
      return g_fastdfs->upload(content, filename, storage_path, true);
    }
    // 本地磁盘
    storage_path = buildStoragePath(file_hash);
    std::string dir_part = storage_path.substr(0, storage_path.find_last_of('/'));
    if (!ensureDirectory(dir_part))
      return false;
    std::ofstream out(storage_path, std::ios::binary);
    if (!out)
      return false;
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return true;
  }

  // 判断是否为 FastDFS file_id（以 "group" 开头）
  bool isFastDfsPath(const std::string &path)
  {
    return path.find("group") == 0 || path.find("Group") == 0;
  }

  // 将 FastDFS file_id 转为 Nginx X-Accel-Redirect 内部 URL
  // file_id:  "group1/M00/00/00/filename"
  // 返回:    "/fastdfs-files/00/00/filename"
  std::string toFastDfsRedirectUrl(const std::string &file_id)
  {
    // 跳过前两段 "group1/M00/" → 保留 "00/00/filename"
    size_t pos = file_id.find('/');
    if (pos == std::string::npos)
      return "";
    pos = file_id.find('/', pos + 1);
    if (pos == std::string::npos)
      return "";
    return "/fastdfs-files/" + file_id.substr(pos + 1);
  }

  // 读取文件 / 从 FastDFS 下载
  bool retrieveFile(const std::string &storage_path, std::string &content)
  {
    if (isFastDfsPath(storage_path) && g_fastdfs && g_fastdfs->initialized())
    {
      return g_fastdfs->download(storage_path, content);
    }
    // 本地磁盘
    std::ifstream file(storage_path, std::ios::binary | std::ios::ate);
    if (!file)
      return false;
    auto size = file.tellg();
    file.seekg(0);
    content.resize(static_cast<size_t>(size));
    file.read(content.data(), size);
    return true;
  }

  // 删除文件 / 从 FastDFS 删除
  bool deleteStoredFile(const std::string &storage_path)
  {
    if (isFastDfsPath(storage_path) && g_fastdfs && g_fastdfs->initialized())
    {
      return g_fastdfs->remove(storage_path);
    }
    // 本地磁盘
    if (!storage_path.empty() && std::filesystem::exists(storage_path))
    {
      std::error_code ec;
      std::filesystem::remove(storage_path, ec);
      return !ec;
    }
    return true;
  }

  // ─── 路由处理器 ──────────────────────────────────

  // GET /health
  void handleHealth(const HttpRequest & /*req*/, HttpResponse &resp)
  {
    json j;
    j["status"] = "ok";

    auto &pool = ConnectionPool::instance();
    j["database"]["available"] = static_cast<int64_t>(pool.available());
    j["database"]["active"] = static_cast<int64_t>(pool.active());
    j["database"]["status"] = (pool.available() > 0 || pool.active() > 0) ? "connected" : "disconnected";

    auto &cm = CacheManager::instance();
    j["cache"]["type"] = cm.type();
    j["cache"]["initialized"] = cm.initialized();

    resp.contentType = "application/json";
    resp.body = j.dump();
  }

  // POST /api/user/register
  void handleRegister(const HttpRequest &req, HttpResponse &resp)
  {
    // 限流：每个 IP 每 60 秒最多 10 次注册请求
    std::string client_ip = req.getHeader("X-Forwarded-For");
    if (client_ip.empty()) client_ip = req.getHeader("Remote-Addr");
    if (client_ip.empty()) client_ip = "unknown";
    if (!g_rate_limiter.allow(client_ip))
    {
      makeJsonResponse(resp, 429, "too many requests, please try later");
      return;
    }
    const json *json_body = requireJsonBody(req, resp);
    if (!json_body)
      return;

    RegisterRequest reg_req{
        (*json_body)["username"].get<std::string>(),
        (*json_body)["password"].get<std::string>(),
        (*json_body)["email"].get<std::string>(),
    };

    AuthService auth_service;
    auto result = auth_service.registerUser(reg_req);
    if (!result.success)
    {
      makeJsonResponse(resp, 400, result.message);
      return;
    }

    makeJsonResponse(resp, 0, "register success");
  }

  // POST /api/user/login
  void handleLogin(const HttpRequest &req, HttpResponse &resp) {
    // 限流：每个 IP 每 60 秒最多 10 次登录请求
    std::string client_ip = req.getHeader("X-Forwarded-For");
    if (client_ip.empty()) client_ip = req.getHeader("Remote-Addr");
    if (client_ip.empty()) client_ip = "unknown";
    if (!g_rate_limiter.allow(client_ip))
    {
      makeJsonResponse(resp, 429, "too many requests, please try later");
      return;
    }
    const json *json_body = requireJsonBody(req, resp);
    if (!json_body)
      return;

    LoginRequest login_req{
        (*json_body)["username"].get<std::string>(),
        (*json_body)["password"].get<std::string>(),
    };

    AuthService auth_service;
    auto result = auth_service.login(login_req);
    if (!result.success)
    {
      makeJsonResponse(resp, 401, result.message);
      return;
    }

    std::string token = PasswordHasher::generateToken(32);

    auto now = std::chrono::system_clock::now();
    auto expires = now + std::chrono::hours(24 * 7);
    auto expires_t = std::chrono::system_clock::to_time_t(expires);
    char expires_buf[32];
    std::strftime(expires_buf, sizeof(expires_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&expires_t));

    SessionDao session_dao;
    if (!session_dao.createSession(result.data->id, token, expires_buf))
    {
      logError("handleLogin: failed to create session");
      makeJsonResponse(resp, 500, "failed to create session");
      return;
    }

    json j;
    j["code"] = 0;
    j["message"] = "login success";
    j["token"] = token;
    j["user_id"] = result.data->id;
    resp.contentType = "application/json";
    resp.body = j.dump();
  }

  // POST /api/user/logout
  void handleLogout(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = requireAuth(req, resp);
    if (user_id == 0)
      return;

    const auto &auth_header = req.getHeader("Authorization");
    std::string token = auth_header.substr(7); // 跳过 "Bearer "

    SessionDao session_dao;
    session_dao.deleteByToken(token);

    makeJsonResponse(resp, 0, "logout success");
  }

  // POST /api/files/upload
  void handleUpload(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = requireAuth(req, resp);
    if (user_id == 0)
      return;

    // 解析 multipart/form-data
    std::string ct = req.getHeader("Content-Type");
    size_t bpos = ct.find("boundary=");
    if (bpos == std::string::npos)
    {
      makeJsonResponse(resp, 400, "Content-Type must be multipart/form-data");
      return;
    }
    std::string boundary = ct.substr(bpos + 9);
    auto files = parseMultipartFormData(req.body, boundary);

    if (files.empty())
    {
      makeJsonResponse(resp, 400, "no file uploaded");
      return;
    }

    auto &upload_file = files[0];
    std::string filename = upload_file.filename;
    std::string file_content = std::move(upload_file.content);

    if (file_content.empty())
    {
      makeJsonResponse(resp, 400, "empty file");
      return;
    }

    // 计算 SHA-256 哈希
    auto hash_bytes = sha256(file_content);
    std::string file_hash = toHexString(hash_bytes);

    // 检查文件是否已存在（去重）
    FileService file_service;
    auto existing = file_service.getFileByHash(file_hash);
    if (existing.success)
    {
      auto &file = *existing.data;
      json j;
      j["code"] = 0;
      j["message"] = "file already exists";
      j["file_id"] = file.id;
      j["hash"] = file.file_hash;
      resp.contentType = "application/json";
      resp.body = j.dump();
      return;
    }

    // 存储到后端（本地磁盘或 FastDFS）
    std::string storage_path;
    if (!storeFile(file_content, filename, file_hash, storage_path))
    {
      logError("handleUpload: failed to store file");
      makeJsonResponse(resp, 500, "failed to store file");
      return;
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
      std::filesystem::remove(storage_path);
      makeJsonResponse(resp, 500, "failed to create file record");
      return;
    }

    logInfo("File uploaded: user=" + std::to_string(user_id) +
            ", filename=" + filename +
            ", size=" + std::to_string(record.file_size) +
            ", hash=" + file_hash);

    // ─── AI 处理：文本提取 → 分块 → 向量化 → 索引 ──────
    // 注意：AI 处理失败不影响上传成功
    auto &created_file = *create_result.data;
    std::int64_t created_file_id = created_file.id;

    auto extract_result = TextExtractor::extract(file_content, filename);
    if (extract_result.success)
    {
        // 分块
        auto chunks = TextChunker::chunkByParagraph(
            extract_result.text, filename, created_file_id, 1000);

        if (!chunks.empty())
        {
            logInfo("AI: extracted " + std::to_string(extract_result.text.size()) +
                    " chars, " + std::to_string(chunks.size()) + " chunks");

            // 逐块生成 embedding 并加入向量索引
            int indexed = 0;
            for (auto &chunk : chunks)
            {
                auto embed_result = g_llm_service->embed(chunk.content);
                if (embed_result.success && embed_result.data)
                {
                    VectorStore::Document doc;
                    doc.chunk_id = std::to_string(created_file_id) + ":" + std::to_string(chunk.index);
                    doc.file_id = created_file_id;
                    doc.content = chunk.content;
                    doc.filename = filename;
                    doc.embedding = std::move(*embed_result.data);
                    doc.chunk_index = chunk.index;
                    g_vector_store.addDocument(std::move(doc));
                    indexed++;
                }
            }

            logInfo("AI: indexed " + std::to_string(indexed) + " chunks for file " +
                    std::to_string(created_file_id));

            // 可选：生成摘要和标签（使用第一个块作为摘要源）
            if (!chunks.empty())
            {
                std::string first_chunk = chunks[0].content.substr(0, 2000);
                auto summary_result = g_llm_service->analyze(
                    first_chunk,
                    "用一句话简要概括这个文件的内容，不超过50字。");
                auto tags_result = g_llm_service->analyze(
                    first_chunk,
                    "为这个文件生成3-5个关键词标签，用逗号分隔，不要多余文字。");

                std::string summary;
                std::string tags;

                if (summary_result.success && summary_result.data)
                    summary = std::move(*summary_result.data);
                if (tags_result.success && tags_result.data)
                    tags = std::move(*tags_result.data);

                // 更新数据库中的摘要和标签
                if (!summary.empty() || !tags.empty())
                {
                    FileDao file_dao;
                    file_dao.updateFileMeta(created_file_id, summary, tags);
                    logInfo("AI: updated meta for file " + std::to_string(created_file_id));
                }
            }
        }
    }
    else
    {
        logInfo("AI: no extractable content for file " + std::to_string(created_file_id) +
                " (" + filename + ")");
    }

    json j;
    j["code"] = 0;
    j["message"] = "upload success";
    j["file_id"] = created_file_id;
    j["hash"] = file_hash;
    resp.contentType = "application/json";
    resp.body = j.dump();
  }

  // ─── 分片上传 ──────────────────────────────────

  // POST /api/files/upload/init
  void handleChunkInit(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = requireAuth(req, resp);
    if (user_id == 0)
      return;

    const json *json_body = requireJsonBody(req, resp);
    if (!json_body)
      return;

    std::string filename = (*json_body)["filename"].get<std::string>();
    std::int64_t file_size = (*json_body)["file_size"].get<int64_t>();
    int total_chunks = (*json_body)["total_chunks"].get<int>();
    int chunk_size = (*json_body)["chunk_size"].get<int>();

    if (filename.empty() || file_size <= 0 || total_chunks <= 0 || chunk_size <= 0)
    {
      makeJsonResponse(resp, 400, "invalid parameters");
      return;
    }

    std::string upload_id = ChunkUploadManager::instance().createSession(
        user_id, filename, file_size, total_chunks, chunk_size);

    json j;
    j["code"] = 0;
    j["message"] = "init success";
    j["upload_id"] = upload_id;
    resp.contentType = "application/json";
    resp.body = j.dump();
  }

  // POST /api/files/upload/{uploadId}/{chunkIndex}
  void handleChunkUpload(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = requireAuth(req, resp);
    if (user_id == 0)
      return;

    std::string uploadId = req.getParam("uploadId");
    std::string chunkIndexStr = req.getParam("chunkIndex");

    auto session = ChunkUploadManager::instance().getSession(uploadId);
    if (!session || session->user_id != user_id)
    {
      makeJsonResponse(resp, 404, "upload session not found");
      return;
    }

    int chunk_index = std::stoi(chunkIndexStr);
    if (chunk_index < 0 || chunk_index >= session->total_chunks)
    {
      makeJsonResponse(resp, 400, "invalid chunk index");
      return;
    }

    // 请求体就是分片数据
    const std::string &chunk_data = req.body;
    if (chunk_data.empty())
    {
      makeJsonResponse(resp, 400, "empty chunk");
      return;
    }

    // 写入临时目录
    std::string tmp_dir = "uploads/tmp/" + uploadId;
    std::filesystem::create_directories(tmp_dir);

    std::string chunk_path = tmp_dir + "/" + chunkIndexStr;
    {
      std::ofstream out(chunk_path, std::ios::binary);
      if (!out)
      {
        makeJsonResponse(resp, 500, "failed to write chunk");
        return;
      }
      out.write(chunk_data.data(), chunk_data.size());
    }

    ChunkUploadManager::instance().markChunkReceived(uploadId, chunk_index);

    logDebug("ChunkUpload: received chunk " + chunkIndexStr + "/" +
             std::to_string(session->total_chunks) + " for " + uploadId);

    makeJsonResponse(resp, 0, "chunk received");
  }

  // POST /api/files/upload/{uploadId}/complete
  void handleChunkComplete(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = requireAuth(req, resp);
    if (user_id == 0)
      return;

    std::string uploadId = req.getParam("uploadId");

    auto session = ChunkUploadManager::instance().getSession(uploadId);
    if (!session || session->user_id != user_id)
    {
      makeJsonResponse(resp, 404, "upload session not found");
      return;
    }

    if (static_cast<int>(session->received.size()) != session->total_chunks)
    {
      json j;
      j["code"] = 400;
      j["message"] = "not all chunks received";
      j["received"] = static_cast<int>(session->received.size());
      j["total"] = session->total_chunks;
      resp.contentType = "application/json";
      resp.body = j.dump();
      return;
    }

    std::string tmp_dir = "uploads/tmp/" + uploadId;

    // 按 index 顺序合并文件
    std::string merged_path = "uploads/tmp/" + uploadId + "_merged";
    {
      std::ofstream merged(merged_path, std::ios::binary);
      if (!merged)
      {
        makeJsonResponse(resp, 500, "failed to create merged file");
        return;
      }

      for (int i = 0; i < session->total_chunks; i++)
      {
        std::string chunk_path = tmp_dir + "/" + std::to_string(i);
        std::ifstream chunk(chunk_path, std::ios::binary);
        if (!chunk)
        {
          makeJsonResponse(resp, 500, "failed to read chunk " + std::to_string(i));
          return;
        }
        merged << chunk.rdbuf();
      }
    }

    // 计算 SHA-256
    std::string merged_content;
    {
      std::ifstream merged_in(merged_path, std::ios::binary | std::ios::ate);
      if (!merged_in)
      {
        makeJsonResponse(resp, 500, "failed to read merged file");
        return;
      }
      auto size = merged_in.tellg();
      merged_in.seekg(0);
      merged_content.resize(static_cast<size_t>(size));
      merged_in.read(merged_content.data(), size);
    }

    auto hash_bytes = sha256(merged_content);
    std::string file_hash = toHexString(hash_bytes);

    // 存储到后端（本地磁盘或 FastDFS）
    std::string storage_path;
    if (!storeFile(merged_content, session->filename, file_hash, storage_path))
    {
      logError("handleChunkComplete: failed to store merged file");
      makeJsonResponse(resp, 500, "failed to store merged file");
      return;
    }
    // 清理合并临时文件
    std::error_code ec;
    std::filesystem::remove(merged_path, ec);

    // 创建文件记录
    FileRecord record;
    record.user_id = user_id;
    record.filename = session->filename;
    record.file_hash = file_hash;
    record.file_size = session->file_size;
    record.storage_path = storage_path;
    record.status = 1;

    FileService file_service;
    auto result = file_service.createFile(record);

    // 清理临时目录
    std::filesystem::remove_all(tmp_dir);
    ChunkUploadManager::instance().removeSession(uploadId);

    if (!result.success)
    {
      logError("handleChunkComplete: " + result.message);
      makeJsonResponse(resp, 500, result.message);
      return;
    }

    logInfo("ChunkUpload: completed id=" + uploadId +
            " file_id=" + std::to_string(result.data->id) +
            " hash=" + file_hash);

    json j;
    j["code"] = 0;
    j["message"] = "upload complete";
    j["file_id"] = result.data->id;
    j["hash"] = file_hash;
    resp.contentType = "application/json";
    resp.body = j.dump();
  }

  // GET /api/files/upload/{uploadId}/status
  void handleChunkStatus(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = requireAuth(req, resp);
    if (user_id == 0)
      return;

    std::string uploadId = req.getParam("uploadId");

    auto session = ChunkUploadManager::instance().getSession(uploadId);
    if (!session || session->user_id != user_id)
    {
      makeJsonResponse(resp, 404, "upload session not found");
      return;
    }

    json received_json = json::array();
    for (int idx : session->received)
    {
      received_json.push_back(idx);
    }

    json j;
    j["code"] = 0;
    j["message"] = "query success";
    j["upload_id"] = uploadId;
    j["filename"] = session->filename;
    j["file_size"] = session->file_size;
    j["total_chunks"] = session->total_chunks;
    j["received_chunks"] = static_cast<int>(session->received.size());
    j["chunks"] = received_json;

    resp.contentType = "application/json";
    resp.body = j.dump();
  }

  // GET /api/files
  void handleListFiles(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = requireAuth(req, resp);
    if (user_id == 0)
      return;

    int limit = 20;
    int offset = 0;
    auto limit_str = req.getQuery("limit");
    auto offset_str = req.getQuery("offset");
    if (!limit_str.empty())
      limit = std::stoi(limit_str);
    if (!offset_str.empty())
      offset = std::stoi(offset_str);

    FileService file_service;
    auto result = file_service.listUserFiles(user_id, limit, offset);
    if (!result.success)
    {
      makeJsonResponse(resp, 400, result.message);
      return;
    }

    json files_json = json::array();
    for (const auto &f : result.data)
    {
      json item;
      item["id"] = f.id;
      item["filename"] = f.filename;
      item["file_hash"] = f.file_hash;
      item["file_size"] = f.file_size;
      item["status"] = f.status;
      item["created_at"] = f.created_at;
      files_json.push_back(item);
    }

    json j;
    j["code"] = 0;
    j["message"] = "query success";
    j["files"] = files_json;
    j["total"] = static_cast<int>(result.data.size());

    resp.contentType = "application/json";
    resp.body = j.dump();
  }

  // GET /api/files/{fileId}
  void handleDownload(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = requireAuth(req, resp);
    if (user_id == 0)
      return;

    std::string fileIdStr = req.getParam("fileId");
    std::int64_t file_id = std::stoll(fileIdStr);

    FileService file_service;
    auto result = file_service.getFileById(file_id);
    if (!result.success)
    {
      makeJsonResponse(resp, 404, "file not found");
      return;
    }

    auto &file = *result.data;
    if (file.user_id != user_id)
    {
      makeJsonResponse(resp, 403, "access denied");
      return;
    }

    // 如果配置了 FastDFS 数据路径且文件在 FastDFS 上，使用 X-Accel-Redirect
    if (isFastDfsPath(file.storage_path) && !g_fastdfs_data_path.empty())
    {
      std::string redirect_url = toFastDfsRedirectUrl(file.storage_path);
      if (!redirect_url.empty())
      {
        resp.statusCode = 200;
        resp.setHeader("X-Accel-Redirect", redirect_url);
        resp.setHeader("Content-Disposition",
                       "attachment; filename=\"" + file.filename + "\"");
        // body stays empty - Nginx will serve the file
        return;
      }
    }

    // 回退：通过应用读取文件（本地磁盘或 FastDFS API）
    std::string file_content;
    if (!retrieveFile(file.storage_path, file_content))
    {
      logError("handleDownload: file not found: " + file.storage_path);
      makeJsonResponse(resp, 404, "file not found");
      return;
    }

    resp.body = std::move(file_content);
    resp.contentType = "application/octet-stream";
    resp.setHeader("Content-Disposition",
                   "attachment; filename=\"" + file.filename + "\"");
  }

  // DELETE /api/files/{fileId}
  void handleDelete(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = requireAuth(req, resp);
    if (user_id == 0)
      return;

    std::string fileIdStr = req.getParam("fileId");
    std::int64_t file_id = std::stoll(fileIdStr);

    FileService file_service;
    auto file_result = file_service.getFileById(file_id);
    if (!file_result.success)
    {
      makeJsonResponse(resp, 404, "file not found");
      return;
    }

    if (file_result.data->user_id != user_id)
    {
      makeJsonResponse(resp, 403, "access denied");
      return;
    }

    const auto &storage_path = file_result.data->storage_path;
    if (!storage_path.empty())
    {
      if (!deleteStoredFile(storage_path))
      {
        logWarn("handleDelete: failed to delete file: " + storage_path);
      }
    }

    auto status_result = file_service.removeFile(file_id);
    if (!status_result.success)
    {
      makeJsonResponse(resp, 500, status_result.message);
      return;
    }

    logInfo("File deleted: user=" + std::to_string(user_id) + ", file_id=" + std::to_string(file_id));
    makeJsonResponse(resp, 0, "delete success");
  }

  // ─── 分享链接 ──────────────────────────────────

  // POST /api/shares
  void handleShareCreate(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = requireAuth(req, resp);
    if (user_id == 0)
      return;

    const json *json_body = requireJsonBody(req, resp);
    if (!json_body)
      return;

    std::int64_t file_id = (*json_body)["file_id"].get<int64_t>();
    int expire_hours = (*json_body)["expire_hours"].get<int>();

    ShareService share_service;
    auto result = share_service.createShare(user_id, file_id, expire_hours);
    if (!result.success)
    {
      makeJsonResponse(resp, 400, result.message);
      return;
    }

    json j;
    j["code"] = 0;
    j["message"] = "share created";
    j["share_token"] = *result.data;
    j["url"] = "/api/shares/" + *result.data;
    resp.contentType = "application/json";
    resp.body = j.dump();
  }

  // DELETE /api/shares/{token}
  void handleShareDelete(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = requireAuth(req, resp);
    if (user_id == 0)
      return;

    std::string token = req.getParam("token");

    ShareDao share_dao;
    auto share = share_dao.findByToken(token);
    if (!share.has_value())
    {
      makeJsonResponse(resp, 404, "share not found");
      return;
    }
    if (share->user_id != user_id)
    {
      makeJsonResponse(resp, 403, "access denied");
      return;
    }

    share_dao.deleteByToken(token);
    makeJsonResponse(resp, 0, "share deleted");
  }

  // GET /api/shares
  void handleShareList(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = requireAuth(req, resp);
    if (user_id == 0)
      return;

    ShareDao share_dao;
    auto shares = share_dao.listByUserId(user_id);

    json shares_json = json::array();
    for (const auto &s : shares)
    {
      json item;
      item["id"] = s.id;
      item["file_id"] = s.file_id;
      item["share_token"] = s.share_token;
      item["url"] = "/api/shares/" + s.share_token;
      item["expires_at"] = s.expires_at;
      item["created_at"] = s.created_at;
      shares_json.push_back(item);
    }

    json j;
    j["code"] = 0;
    j["message"] = "query success";
    j["shares"] = shares_json;
    j["total"] = static_cast<int>(shares.size());

    resp.contentType = "application/json";
    resp.body = j.dump();
  }

  // POST /api/shares/{token}/save
  void handleShareSave(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = requireAuth(req, resp);
    if (user_id == 0)
      return;

    std::string token = req.getParam("token");

    // 查找分享
    ShareDao share_dao;
    auto share = share_dao.findByToken(token);
    if (!share.has_value())
    {
      makeJsonResponse(resp, 404, "share not found or expired");
      return;
    }

    // 不能转存自己的文件
    if (share->user_id == user_id)
    {
      makeJsonResponse(resp, 400, "cannot save your own share");
      return;
    }

    // 获取原文件
    FileService file_service;
    auto file_result = file_service.getFileById(share->file_id);
    if (!file_result.success)
    {
      makeJsonResponse(resp, 404, "original file not found");
      return;
    }

    auto &original = *file_result.data;

    // 检查是否已转存过（同一用户 + 同一文件哈希）
    auto existing = file_service.getFileByHash(original.file_hash);
    if (existing.success)
    {
      auto &dup = *existing.data;
      if (dup.user_id == user_id)
      {
        json j;
        j["code"] = 0;
        j["message"] = "file already saved";
        j["file_id"] = dup.id;
        resp.contentType = "application/json";
        resp.body = j.dump();
        return;
      }
    }

    // 创建新文件记录（指向同一存储路径）
    FileRecord new_record;
    new_record.user_id = user_id;
    new_record.filename = original.filename;
    new_record.file_hash = original.file_hash;
    new_record.file_size = original.file_size;
    new_record.storage_path = original.storage_path;
    new_record.status = 1;
    new_record.summary = original.summary;
    new_record.tags = original.tags;

    auto create_result = file_service.createFile(new_record);
    if (!create_result.success)
    {
      makeJsonResponse(resp, 500, "failed to save file");
      return;
    }

    logInfo("Share saved: user=" + std::to_string(user_id) +
            ", file_id=" + std::to_string(share->file_id) +
            ", token=" + token);

    json j;
    j["code"] = 0;
    j["message"] = "file saved successfully";
    j["file_id"] = create_result.data->id;
    resp.contentType = "application/json";
    resp.body = j.dump();
  }

  // GET /api/shares/ranking
  void handleShareRanking(const HttpRequest &req, HttpResponse &resp)
  {
    // 公开接口，不需要认证
    int limit = 10;
    auto limit_str = req.getQuery("limit");
    if (!limit_str.empty())
      limit = std::stoi(limit_str);

    json results = json::array();

    // 尝试从 Redis 获取排行榜
    auto &cm = CacheManager::instance();
    // 检查是否 Redis 模式
    if (cm.type() == "redis")
    {
      // 需要通过 CacheManager 获取 RedisClient
      // 目前 CacheManager 不暴露 RedisClient，我们先用 SQL 查询
      // 这里使用 shares 表的访问计数（如果有的话）
      // 简化实现：使用 SQL 统计分享次数
    }

    // 后备方案：从数据库统计各文件的分享次数
    auto conn = ConnectionPool::instance().getConnection();
    if (conn)
    {
      std::string sql = "SELECT s.file_id, f.filename, f.user_id, u.username, COUNT(*) as share_count "
                        "FROM shares s "
                        "JOIN files f ON s.file_id = f.id "
                        "JOIN users u ON f.user_id = u.id "
                        "GROUP BY s.file_id ORDER BY share_count DESC LIMIT " +
                        std::to_string(limit);
      if (conn->query(sql))
      {
        while (conn->next())
        {
          json item;
          item["file_id"] = conn->value(0) ? std::stoll(conn->value(0)) : 0;
          item["filename"] = conn->value(1) ? conn->value(1) : "";
          item["user_id"] = conn->value(2) ? std::stoll(conn->value(2)) : 0;
          item["username"] = conn->value(3) ? conn->value(3) : "";
          item["share_count"] = conn->value(4) ? std::stoll(conn->value(4)) : 0;
          results.push_back(std::move(item));
        }
      }
    }

    json j;
    j["code"] = 0;
    j["message"] = "success";
    j["ranking"] = results;
    j["total"] = static_cast<int>(results.size());
    resp.contentType = "application/json";
    resp.body = j.dump();
  }

  // ─── 文件秒传确认 ──────────────────────────────────

  // POST /api/files/check
  void handleFileCheck(const HttpRequest &req, HttpResponse &resp)
  {
    const json *json_body = requireJsonBody(req, resp);
    if (!json_body)
      return;

    std::string file_hash = (*json_body)["file_hash"].get<std::string>();
    if (file_hash.empty())
    {
      makeJsonResponse(resp, 400, "file_hash is required");
      return;
    }

    FileService file_service;
    auto existing = file_service.getFileByHash(file_hash);

    json j;
    j["code"] = 0;
    j["message"] = "ok";

    if (existing.success)
    {
      j["data"]["exists"] = true;
      j["data"]["file_id"] = existing.data->id;
      j["data"]["file_size"] = existing.data->file_size;
      j["data"]["filename"] = existing.data->filename;
    }
    else
    {
      j["data"]["exists"] = false;
    }

    resp.contentType = "application/json";
    resp.body = j.dump();
  }

  // ─── LLM ─────────────────────────────────────────

  // POST /api/llm/chat
  void handleLlmChat(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = requireAuth(req, resp);
    if (user_id == 0)
      return;

    const json *json_body = requireJsonBody(req, resp);
    if (!json_body)
      return;

    std::string prompt = (*json_body)["prompt"].get<std::string>();
    if (prompt.empty())
    {
      makeJsonResponse(resp, 400, "prompt is empty");
      return;
    }

    auto result = g_llm_service->chat(prompt);
    if (!result.success)
    {
      makeJsonResponse(resp, 500, result.message);
      return;
    }

    json j;
    j["code"] = 0;
    j["message"] = "success";
    j["response"] = *result.data;
    resp.contentType = "application/json";
    resp.body = j.dump();
  }

  // POST /api/llm/analyze
  void handleLlmAnalyze(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = requireAuth(req, resp);
    if (user_id == 0)
      return;

    const json *json_body = requireJsonBody(req, resp);
    if (!json_body)
      return;

    std::int64_t file_id = (*json_body)["file_id"].get<int64_t>();
    std::string instruction = (*json_body)["instruction"].get<std::string>();

    if (file_id <= 0 || instruction.empty())
    {
      makeJsonResponse(resp, 400, "file_id and instruction are required");
      return;
    }

    FileService file_service;
    auto file = file_service.getFileById(file_id);
    if (!file.success)
    {
      makeJsonResponse(resp, 404, "file not found");
      return;
    }

    if (file.data->user_id != user_id)
    {
      makeJsonResponse(resp, 403, "access denied");
      return;
    }

    std::string file_content;
    {
      std::ifstream in(file.data->storage_path);
      if (!in)
      {
        makeJsonResponse(resp, 500, "failed to read file");
        return;
      }
      std::stringstream ss;
      ss << in.rdbuf();
      file_content = ss.str();
    }

    if (file_content.size() > 100000)
    {
      file_content.resize(100000);
      file_content += "\n\n[内容已截断，仅分析前 100KB]";
    }

    auto result = g_llm_service->analyze(file_content, instruction);
    if (!result.success)
    {
      makeJsonResponse(resp, 500, result.message);
      return;
    }

    json j;
    j["code"] = 0;
    j["message"] = "success";
    j["analysis"] = *result.data;
    resp.contentType = "application/json";
    resp.body = j.dump();
  }

  // ─── 语义搜索 ─────────────────────────────────────────

  // POST /api/search
  void handleSearch(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = requireAuth(req, resp);
    if (user_id == 0)
      return;

    const json *json_body = requireJsonBody(req, resp);
    if (!json_body)
      return;

    std::string query = (*json_body)["query"].get<std::string>();
    if (query.empty())
    {
      makeJsonResponse(resp, 400, "query is required");
      return;
    }

    int limit = (*json_body)["limit"].get<int>();
    if (limit <= 0 || limit > 100)
      limit = 10;

    // 1. 生成查询向量
    auto embed_result = g_llm_service->embed(query);
    if (!embed_result.success || !embed_result.data)
    {
      makeJsonResponse(resp, 500, "failed to generate query embedding");
      return;
    }

    // 2. 在向量存储中搜索
    auto results = g_vector_store.search(*embed_result.data, limit);

    // 3. 按 file_id 去重聚合，只保留最高分的块
    std::map<std::int64_t, VectorStore::SearchResult> best_per_file;
    for (auto &r : results)
    {
      auto it = best_per_file.find(r.file_id);
      if (it == best_per_file.end() || r.score > it->second.score)
      {
        best_per_file[r.file_id] = std::move(r);
      }
    }

    // 4. 获取文件信息并构建响应
    json results_json = json::array();
    FileService file_service;

    for (auto &[fid, sr] : best_per_file)
    {
      auto file_result = file_service.getFileById(fid);
      if (!file_result.success)
        continue;

      auto &file = *file_result.data;

      json item;
      item["file_id"] = file.id;
      item["filename"] = file.filename;
      item["file_size"] = file.file_size;
      item["score"] = sr.score;
      item["snippet"] = sr.content.substr(0, 200); // 只返回片段
      item["summary"] = file.summary;
      item["tags"] = file.tags;
      results_json.push_back(std::move(item));
    }

    json j;
    j["code"] = 0;
    j["message"] = "search success";
    j["results"] = results_json;
    j["total"] = static_cast<int>(results_json.size());
    resp.contentType = "application/json";
    resp.body = j.dump();
  }

  // ─── Agent 对话 ──────────────────────────────────

  // POST /api/agent/chat
  void handleAgentChat(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = requireAuth(req, resp);
    if (user_id == 0)
      return;

    const json *json_body = requireJsonBody(req, resp);
    if (!json_body)
      return;

    std::string message = (*json_body)["message"].get<std::string>();
    if (message.empty())
    {
      makeJsonResponse(resp, 400, "message is required");
      return;
    }

    if (!g_agent_engine.initialized())
    {
      makeJsonResponse(resp, 503, "Agent engine not initialized");
      return;
    }

    auto response = g_agent_engine.chat(message, user_id);

    json j;
    j["code"] = 0;
    j["message"] = "success";
    j["response"] = response;
    resp.contentType = "application/json";
    resp.body = j.dump();
  }

  // ─── Admin ─────────────────────────────────────────

  bool requireAdmin(const HttpRequest &req, HttpResponse &resp)
  {
    std::int64_t user_id = requireAuth(req, resp);
    if (user_id == 0)
      return false;

    UserDao user_dao;
    auto user = user_dao.findById(user_id);
    if (!user.has_value() || !user->is_admin)
    {
      makeJsonResponse(resp, 403, "admin access required");
      return false;
    }
    return true;
  }

  // GET /api/admin/users
  void handleAdminUsers(const HttpRequest &req, HttpResponse &resp)
  {
    if (!requireAdmin(req, resp))
      return;

    int limit = 20, offset = 0;
    auto limit_str = req.getQuery("limit");
    auto offset_str = req.getQuery("offset");
    if (!limit_str.empty())
      limit = std::stoi(limit_str);
    if (!offset_str.empty())
      offset = std::stoi(offset_str);

    UserDao user_dao;
    auto users = user_dao.listAll(limit, offset);
    auto total = user_dao.countAll();

    json users_json = json::array();
    for (const auto &u : users)
    {
      json item;
      item["id"] = u.id;
      item["username"] = u.username;
      item["email"] = u.email;
      item["status"] = u.status;
      item["is_admin"] = u.is_admin;
      item["created_at"] = u.created_at;
      users_json.push_back(item);
    }

    json j;
    j["code"] = 0;
    j["users"] = users_json;
    j["total"] = static_cast<int64_t>(total);

    resp.contentType = "application/json";
    resp.body = j.dump();
  }

  // PATCH /api/admin/users/{userId}/status
  void handleAdminSetStatus(const HttpRequest &req, HttpResponse &resp)
  {
    if (!requireAdmin(req, resp))
      return;

    const json *json_body = requireJsonBody(req, resp);
    if (!json_body)
      return;

    std::string userIdStr = req.getParam("userId");
    std::int64_t user_id = std::stoll(userIdStr);
    int status = (*json_body)["status"].get<int>();

    UserDao user_dao;
    if (!user_dao.updateStatus(user_id, status))
    {
      makeJsonResponse(resp, 500, "failed to update status");
      return;
    }
    makeJsonResponse(resp, 0, "status updated");
  }

  // PATCH /api/admin/users/{userId}/admin
  void handleAdminSetAdmin(const HttpRequest &req, HttpResponse &resp)
  {
    if (!requireAdmin(req, resp))
      return;

    const json *json_body = requireJsonBody(req, resp);
    if (!json_body)
      return;

    std::string userIdStr = req.getParam("userId");
    std::int64_t user_id = std::stoll(userIdStr);
    bool is_admin = (*json_body)["is_admin"].get<bool>();

    auto conn = ConnectionPool::instance().getConnection();
    if (!conn)
    {
      makeJsonResponse(resp, 500, "database error");
      return;
    }
    std::string sql = "UPDATE users SET is_admin = " + std::to_string(is_admin ? 1 : 0) +
                      ", updated_at = NOW() WHERE id = " + std::to_string(user_id);
    if (!conn->update(sql))
    {
      makeJsonResponse(resp, 500, "failed to update admin status");
      return;
    }
    makeJsonResponse(resp, 0, "admin status updated");
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

  CacheManager::instance().init(app_config.cache);

  // 初始化 LLM 服务
  g_llm_service = std::make_unique<LlmService>();
  g_llm_service->init(app_config.llm.provider,
                      app_config.llm.api_key,
                      app_config.llm.api_base,
                      app_config.llm.model,
                      app_config.llm.embedding_model);

  // 初始化 Agent 引擎
  if (g_llm_service->initialized())
  {
    auto llm = g_llm_service->getLLM();
    if (llm)
    {
      g_agent_engine.init(llm, g_vector_store);
      if (g_agent_engine.initialized())
        logInfo("AgentEngine initialized");
      else
        logWarn("AgentEngine init failed");
    }
  }

  // 初始化存储后端（FastDFS）
  if (app_config.storage.type == "fastdfs")
  {
    auto fdfs = std::make_unique<FastDfsClient>();
    if (fdfs->init(app_config.storage.fastdfs_conf))
    {
      g_fastdfs = std::move(fdfs);
      g_fastdfs_data_path = app_config.storage.fastdfs_data_path;
      logInfo("Storage backend: FastDFS (" + app_config.storage.fastdfs_conf + ")");
      if (!g_fastdfs_data_path.empty())
        logInfo("FastDFS X-Accel-Redirect data path: " + g_fastdfs_data_path);
    }
    else
    {
      logError("FastDFS init failed, falling back to local storage");
    }
  }
  else
  {
    logInfo("Storage backend: local disk");
  }

  logInfo("Configuration loaded and validated");
  logDebug(app_config.toString());
  logInfo("MySQL pool initialized: available=" + std::to_string(ConnectionPool::instance().available()) +
          ", active=" + std::to_string(ConnectionPool::instance().active()));

  // ─── HTTP 服务（FastCGI）─────────────────────────

  ensureDirectory("uploads");

  logInfo("Starting FastCGI server");
  logInfo("Database: " + app_config.database.host + ":" + std::to_string(app_config.database.port) +
          "/" + app_config.database.database);

  FastCgiServer server;

  // ─── 注册全部路由 ──────────────────────────────

  // 公共路由
  server.get("/health", handleHealth);
  server.post("/api/user/register", handleRegister);
  server.post("/api/user/login", handleLogin);

  // 受保护路由
  server.post("/api/user/logout", handleLogout);

  // 文件秒传确认（不需认证）
  server.post("/api/files/check", handleFileCheck);

  // 分片上传路由（需在 /api/files/upload 之前注册）
  server.post("/api/files/upload/init", handleChunkInit);
  server.post("/api/files/upload/{uploadId}/complete", handleChunkComplete);
  server.get("/api/files/upload/{uploadId}/status", handleChunkStatus);
  server.post("/api/files/upload/{uploadId}/{chunkIndex}", handleChunkUpload);

  // 文件路由
  server.post("/api/files/upload", handleUpload);
  server.get("/api/files", handleListFiles);
  server.get("/api/files/{fileId}", handleDownload);
  server.del("/api/files/{fileId}", handleDelete);

  // 分享链接
  server.get("/api/shares/{token}", [](const HttpRequest &req, HttpResponse &resp)
             {
        std::string token = req.getParam("token");

        ShareDao share_dao;
        auto share = share_dao.findByToken(token);
        if (!share.has_value())
        {
            makeJsonResponse(resp, 404, "share not found or expired");
            return;
        }

        FileService file_service;
        auto file_result = file_service.getFileById(share->file_id);
        if (!file_result.success)
        {
            makeJsonResponse(resp, 404, "file not found");
            return;
        }

        auto &file = *file_result.data;

        // FastDFS + X-Accel-Redirect（如果配置了）
        if (isFastDfsPath(file.storage_path) && !g_fastdfs_data_path.empty())
        {
            std::string redirect_url = toFastDfsRedirectUrl(file.storage_path);
            if (!redirect_url.empty())
            {
                resp.setHeader("X-Accel-Redirect", redirect_url);
                resp.setHeader("Content-Disposition",
                                "attachment; filename=\"" + file.filename + "\"");
                return;
            }
        }

        std::string file_content;
        if (!retrieveFile(file.storage_path, file_content))
        {
            makeJsonResponse(resp, 404, "file not found");
            return;
        }

        resp.body = std::move(file_content);
        resp.contentType = "application/octet-stream";
        resp.setHeader("Content-Disposition",
                        "attachment; filename=\"" + file.filename + "\""); });

  // 其余分享路由需认证
  server.post("/api/shares", handleShareCreate);
  server.get("/api/shares", handleShareList);
  server.del("/api/shares/{token}", handleShareDelete);
  server.post("/api/shares/{token}/save", handleShareSave);
  server.get("/api/shares/ranking", handleShareRanking);

  // LLM 路由
  server.post("/api/llm/chat", handleLlmChat);
  server.post("/api/llm/analyze", handleLlmAnalyze);

  // 语义搜索
  server.post("/api/search", handleSearch);

  // Agent 对话
  server.post("/api/agent/chat", handleAgentChat);

  // Admin 路由
  server.get("/api/admin/users", handleAdminUsers);
  server.patch("/api/admin/users/{userId}/status", handleAdminSetStatus);
  server.patch("/api/admin/users/{userId}/admin", handleAdminSetAdmin);

  // ─── 启动主循环 ────────────────────────────────
  // FastCGI 监听端口（也可以改为文件 socket，如 /var/run/fileagent.sock）
  std::string listen_port = ":" + std::to_string(app_config.server.port);
  server.setSocketPath(listen_port);
  server.run();

  return 0;
}
