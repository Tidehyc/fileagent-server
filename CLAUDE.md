# FileAgent Server

C++20 AI 智能文件管理服务 — FastCGI + Nginx + MySQL + Redis + LLM

---

## 项目需求文档

### 总体目标
构建一个高性能的 AI 智能文件管理 HTTP 服务，支持用户注册/登录、文件上传/下载/管理、分享/转存、LLM 智能搜索，对标工业级云存储系统。

### 功能需求

#### P0 — 核心 MVP（已完成）

| # | 需求 | 状态 | 说明 |
|---|------|------|------|
| 1 | YAML 配置文件加载与校验 | ✅ Done | `config.yaml` → `AppConfig` |
| 2 | MySQL 连接池 | ✅ Done | 单例模式、RAII 连接守卫 |
| 3 | MySQL 底层封装 | ✅ Done | `MysqlConn` — C API 封装 |
| 4 | SQL 表结构 DDL | ✅ Done | 五张表：users/files/file_chunks/shares/sessions |
| 5 | User DAO (CRUD) | ✅ Done | 含 is_admin 字段 |
| 6 | File DAO (CRUD) | ✅ Done | 按 ID/哈希/用户查询 |
| 7 | 用户注册 | ✅ Done | bcrypt 哈希 + 去重 |
| 8 | 用户登录 | ✅ Done | bcrypt 验证 + Session Token |
| 9 | 文件上传 API | ✅ Done | SHA-256 去重 + 写盘 |
| 10 | 文件下载 API | ✅ Done | 流式输出 + 权限验证 |
| 11 | 文件列表 API | ✅ Done | 分页查询 |
| 12 | 文件删除 API | ✅ Done | 磁盘 + 数据库清理 |
| 13 | 密码哈希存储 | ✅ Done | bcrypt (`crypt_r`) |
| 14 | 认证令牌机制 | ✅ Done | Session 方案 + Bearer Token |
| 15 | HTTP 路由注册 | ✅ Done | 全部 21+ 个路由 |

#### P1 — 重要功能（已完成）

| # | 需求 | 状态 | 说明 |
|---|------|------|------|
| 16 | LRU 缓存 | ✅ Done | 线程安全 + CacheManager |
| 17 | Redis 缓存 | ✅ Done | hiredis + 自动降级 LRU |
| 18 | 文件去重 | ✅ Done | SHA-256 哈希去重 |
| 19 | 大文件分片上传 | ✅ Done | ChunkUploadService + 4 API |
| 20 | 分享链接 | ✅ Done | ShareDao + ShareService + 4 API |
| 21 | 用户状态管理 | ✅ Done | is_admin 角色 + 3 admin API |

#### P2 — 增强功能（部分完成）

| # | 需求 | 状态 | 说明 |
|---|------|------|------|
| 22 | LLM 集成 | ✅ Done | 四种 provider：ollama/openai/anthropic/google |
| 23 | 健康检查增强 | ✅ Done | JSON：database + cache 状态 |
| 24 | 代码质量清理 | ✅ Done | trimCopy/jsonEscape/requireAuth |

---

## 后续规划（对标工业级云存储）

### Phase 1 — 架构升级 ✅ Done

| # | 需求 | 状态 | 说明 |
|---|------|------|------|
| 1 | Drogon → FastCGI + Nginx 迁移 | ✅ Done | Nginx 反向代理 + libfcgi |
| 2 | 改用 OpenSSL SHA-256 | ✅ Done | 移除 trantor/OpenSSL EVP |

### Phase 1.5 — 分布式存储 ✅ Done

| # | 需求 | 状态 | 说明 |
|---|------|------|------|
| 3 | FastDFS 集成 | ✅ Done | Tracker + Storage 架构，替换本地磁盘 |
| 4 | FastDFS 客户端 SDK | ✅ Done | C++ 封装 FastDfsClient，上传/下载/删除 |
| 5 | Nginx + FastDFS 整合 | ✅ Done | X-Accel-Redirect，handler 认证后 Nginx 直接服文件 |
| 6 | 本地文件迁移脚本 | ✅ Done | `scripts/migrate_to_fastdfs.sh`，9 个文件已迁移 |

> 新文件存储在 FastDFS（file_id = `group1/M00/00/00/xxx`），旧文件仍保留在本地 `uploads/{hash_prefix}/{full_hash}`。`storage_path` 字段根据路径前缀自动判断后端：以 `group` 开头则走 FastDFS，否则走本地磁盘。`config.yaml` 中 `storage.type: fastdfs` 可切换回 `local`。

### Phase 2 — AI 智能搜索 ✅ Done

| # | 需求 | 状态 | 说明 |
|---|------|------|------|
| 7 | 文件文本提取 | ✅ Done | `TextExtractor` — TXT/PDF/代码 → 纯文本（pdftotext） |
| 8 | LLM 摘要 + 标签 | ✅ Done | 上传时自动调用 LLM 生成摘要/标签，更新 DB |
| 9 | 语义搜索 API | ✅ Done | `POST /api/search` — 向量 Embedding + 余弦相似度 Top-K |
| 10 | 搜索结果入库 | ✅ Done | summary/tags 持久化到 files 表 + VectorStore 内存索引 |

> 向量检索基于 Ollama Embedding API（`nomic-embed-text`） + 自实现 `VectorStore`（余弦相似度暴力搜索）。  
> Agent 引擎基于 agents-cpp 的 `LLMInterface::chatWithTools()`，注册 4 个工具（semantic_search / get_file_info / list_files / summarize_file）。

### Phase 3 — 功能补齐 ✅ Done

| # | 需求 | 状态 | 说明 |
|---|------|------|------|
| 11 | 转存文件 | ✅ Done | `POST /api/shares/{token}/save` — 分享 → 转存到自己名下 |
| 12 | 分享排行榜 | ✅ Done | `GET /api/shares/ranking` — SQL 统计分享次数 Top-N |
| 13 | 文件秒传确认 | ✅ Done | `POST /api/files/check` — SHA-256 预检，存在则返回 file_id |

### Phase 4 — 部署与质量 ✅ Done

| # | 需求 | 状态 | 说明 |
|---|------|------|------|
| 14 | 单元测试 | ✅ Done | Google Test + 25 tests（TextExtractor/VectorStore/TextChunker） |
| 15 | Rate limiting | ✅ Done | 滑动窗口限流器，注册/登录 10 次/分钟/IP |
| 16 | Prepared Statement | ✅ Done | PreparedStatement RAII 封装 + UserDao 演示迁移 |
| 17 | Docker + K8s | ✅ Done | Dockerfile 多阶段构建 + docker-compose.yml |

---

## 项目进度

### 当前状态：FastCGI + Nginx 架构升级完成

```
业务逻辑层 ██████████████████ 100%
  ├─ DAO 层 (User/File/Session/Share)  ████████████ 100%
  ├─ Service 层 (Auth/File/Chunk/Share) ████████████ 100%
  ├─ 缓存层 (LRU/Redis)                 ████████████ 100%
  └─ LLM 集成 (4 provider)              ████████████ 100%

HTTP 层    ██████████████████ 100%
  ├─ 业务逻辑 handler                     ████████████ 100%
  └─ HTTP 框架 (FastCGI + Nginx)         ████████████ 100%

部署层     ████░░░░░░░░░░░░ 15%
  ├─ Nginx                               ████████████ 100%
  ├─ Docker                              ░░░░░░░░░░░░   0%
  └─ K8s                                 ░░░░░░░░░░░░   0%

AI 层      ██████████████████ 100%
  ├─ 文本提取 (TextExtractor)           ████████████ 100%
  ├─ 向量索引 (VectorStore)             ████████████ 100%
  ├─ 语义搜索 (Embedding + /api/search) ████████████ 100%
  ├─ 摘要标签 (LLM 自动生成)            ████████████ 100%
  └─ Agent 引擎 (4 tools)               ████████████ 100%
```

---

## 架构说明

### 当前架构

```
客户端 → Nginx (端口 80, 反向代理)
              ├── /api/* → FastCGI → fileagent-server (TCP :10086)
              ├── /health → FastCGI → fileagent-server
              └── /uploads/* → Nginx 直接返回静态文件
```

### 架构图

```
config.yaml → AppConfig → ConnectionPool.init()
                              │
    Nginx (反向代理) ──→ FastCGI 路由分发
         │
         ├── POST /api/user/register (public)
         ├── POST /api/user/login    (public) → AuthService → UserDao → bcrypt
         ├── POST /api/user/logout   → SessionDao.deleteByToken(SHA-256) ...
         │
         ├── POST /api/files/upload  → SHA-256 去重 + 磁盘写入
         ├── GET  /api/files         → FileService.listUserFiles (分页)
         ├── GET  /api/files/{id}    → FileService.getFileById + 读盘
         ├── DELETE /api/files/{id}  → FileService.removeFile + 磁盘清理
         │
         ├── POST /api/files/upload/init       \ 分片上传
         ├── POST /api/files/upload/{id}/{idx}  ├ ChunkUploadManager
         ├── POST /api/files/upload/{id}/complete  /
         ├── GET  /api/files/upload/{id}/status /
         │
         ├── POST /api/shares       \ 分享链接
         ├── GET  /api/shares/{token} ├ ShareService + ShareDao
         ├── DELETE /api/shares/{token}/
         └── GET  /api/shares       /
         │
         ├── POST /api/llm/chat     \ LLM
         ├── POST /api/llm/analyze   ├ createLLM("ollama"/"openai"/...)
         │
         ├── GET  /api/admin/users   \ Admin
         ├── PATCH /api/admin/users/{id}/status ┤ UserDao
         ├── PATCH /api/admin/users/{id}/admin  /
         │
         └── GET  /health           (public) → JSON 状态
```

---

## 目录结构

```
├── CMakeLists.txt          构建配置
├── config.yaml             运行配置
├── nginx.conf              Nginx 反向代理配置
├── sql/
│   └── init.sql            数据库初始化 DDL
├── src/
│   ├── main.cpp            入口：加载配置 → 初始化 → FastCGI 主循环
│   ├── agent/
│   │   └── AgentEngine.h/cpp  ReAct Loop + 工具注册（基于 agents-cpp）
│   ├── common/
│   │   ├── Config.h        AppConfig 及其子结构体
│   │   ├── ConfigLoader.h/cpp YAML 解析 + 校验
│   │   ├── Hash.h/cpp      bcrypt 密码哈希 + 令牌生成
│   │   ├── Sha256.h/cpp    OpenSSL SHA-256 封装
│   │   ├── TextExtractor.h/cpp 文件文本提取（TXT/PDF/代码）
│   │   ├── Logger.h        内联日志
│   │   ├── StringUtil.h    trimCopy / jsonEscape
│   │   ├── Types.h         ApiResponse
│   │   ├── CacheInterface.h / CacheManager.h/cpp / LruCache.h / RedisCache.h/cpp
│   ├── vector/
│   │   ├── VectorStore.h/cpp   内存向量索引 + 余弦相似度 Top-K
│   │   └── TextChunker.h/cpp   文档分块器（滑动窗口 / 按段落）
│   ├── db/
│   │   ├── MysqlConn.h/cpp MySQL C API 封装
│   │   └── ConnectionPool.h/cpp 连接池
│   ├── dao/
│   │   ├── Models.h        数据模型
│   │   ├── UserDao.h/cpp / FileDao.h/cpp / SessionDao.h/cpp / ShareDao.h/cpp
│   ├── service/
│   │   ├── AuthService.h/cpp / FileService.h/cpp / ChunkUploadService.h/cpp
│   │   ├── ShareService.h/cpp / LlmService.h/cpp / ServiceTypes.h
│   └── server/
│       ├── HttpContext.h   HttpRequest / HttpResponse 结构体
│       └── FastCgiServer.h/cpp FastCGI 主循环 + 路由分发
└── uploads/                文件存储根目录
```

---

## 技术选型

| 层面 | 选型 | 说明 |
|------|------|------|
| 语言 | C++20 | |
| HTTP 服务器 | Nginx | 反向代理 + 负载均衡 + 静态文件 |
| 应用协议 | FastCGI (libfcgi) | Nginx ↔ C++ 程序通信 |
| 数据库 | MySQL 8.0+ | utf8mb4 |
| 连接池 | 自实现 | 生产者-消费者 |
| 缓存 | LRU + Redis | 运行时切换 |
| LLM | agents.cpp-main | ollama / openai / anthropic / google |
| 构建 | CMake 3.20+ | vcpkg |
| 配置 | YAML | yaml-cpp |
| JSON | nlohmann-json | 替代 jsoncpp |
| 哈希 | bcrypt (密码) + OpenSSL SHA-256 (文件) | |

---

## 代码约定

### 代码风格
- 命名空间: `fileagent`（小写）
- 类: PascalCase（`AuthService`）
- 函数/变量: camelCase（`getConnection`）
- 成员变量: snake_case_ 后缀下划线（`connection_`）
- 文件命名: PascalCase（`ConnectionPool.h`）

### 代码质量注意
- ✅ `trimCopy` 已抽取到 `src/common/StringUtil.h`（含 `jsonEscape`）
- ✅ 认证/验证代码已抽取为 `requireAuth` / `requireJsonBody`
- SQL 使用 `escape()` 防注入，后续应迁移到 prepared statement
- 当前无异常安全保证，连接池 `shutdown()` 是唯一清理入口
- 所有 DAO 方法都从连接池取连接，没有事务协调能力

---

## 构建与运行

```bash
# 依赖
sudo apt install libfcgi-dev nginx
# vcpkg: yaml-cpp, openssl, hiredis

# 构建
cmake -B build
cmake --build build

# 运行 FastCGI 服务（监听 Unix Socket）
./build/fileagent-server config.yaml

# 配置 Nginx
sudo cp nginx.conf /etc/nginx/sites-available/fileagent
sudo ln -s /etc/nginx/sites-available/fileagent /etc/nginx/sites-enabled/
sudo systemctl reload nginx

# 测试
curl http://localhost/health
```

### 依赖
- yaml-cpp（vcpkg）
- nlohmann-json（vcpkg，header-only）
- MySQL 客户端库（系统: `libmysqlclient-dev`）
- hiredis（系统: `libhiredis-dev`）
- OpenSSL（vcpkg: `openssl`）
- FastCGI（系统: `libfcgi-dev`）
- Nginx（系统: `nginx`）
- FastDFS（系统: `libfdfsclient-dev`，或源码编译）
- agents.cpp-main（外部，链接 `.so`）

---

## 部署备忘
- 生产需创建 MySQL database `fileagent` + 用户 `fileagent`
- `config.yaml` 中的密码/密钥不应提交到版本控制
- `uploads/` 目录需确保运行时有写入权限
- Nginx 配置 `client_max_body_size` 根据需要调整
- FastCGI 建议通过 `systemd` 管理进程生命周期
