# FileAgent Server

C++20 AI 智能文件管理服务 — 基于 Drogon (epoll) + MySQL + llama.cpp

---

## 项目需求文档

### 总体目标
构建一个高性能的 AI 智能文件管理 HTTP 服务，支持用户注册/登录、文件上传/下载/管理，并集成 LLM 实现智能文件处理能力。

### 功能需求

#### P0 — 核心 MVP（完成方可上线）

| # | 需求 | 状态 | 说明 |
|---|------|------|------|
| 1 | YAML 配置文件加载与校验 | ✅ Done | `config.yaml` → `AppConfig`，服务端/数据库/缓存/LLM 配置 |
| 2 | MySQL 连接池 | ✅ Done | 单例模式、RAII 连接守卫、生产者/回收者线程 |
| 3 | MySQL 底层封装 | ✅ Done | `MysqlConn` — C API 封装，支持事务/转义/查询 |
| 4 | SQL 表结构 DDL | ✅ Done | `init.sql` — users/files/file_chunks/shares/sessions 五张表，含外键/索引/字符集 |
| 5 | User DAO (CRUD) | ✅ Done | 建用户、查（ID/用户名）、改状态/密码 |
| 6 | File DAO (CRUD) | ✅ Done | 建文件记录、查（ID/哈希/用户列表）、删 |
| 7 | 用户注册 | ✅ Done | `AuthService::register` — 用户名/密码/邮箱校验 + 去重，密码 bcrypt 哈希存储 |
| 8 | 用户登录 | ✅ Done | `AuthService::login` — bcrypt 密码验证 + Session Token 签发 |
| 9 | 文件上传 API | ✅ Done | HTTP POST multipart/form-data，SHA-256 去重，写盘 + 入库 |
| 10 | 文件下载 API | ✅ Done | HTTP GET 流式文件输出，权限验证 |
| 11 | 文件列表 API | ✅ Done | HTTP GET 分页查询，返回 JSON 数组 |
| 12 | 文件删除 API | ✅ Done | HTTP DELETE 磁盘文件清理 + 数据库记录删除 |
| 13 | 密码哈希存储 | ✅ Done | bcrypt (`crypt_r` + 随机盐，由系统 `libcrypt` 提供) |
| 14 | 认证令牌机制 | ✅ Done | Session 方案：`sessions` 表 + `Bearer <token>` 鉴权，`AuthMiddleware` 全局中间件 |
| 15 | HTTP 路由注册 | ✅ Done | 全部 8 个路由注册：register/login/logout + upload/download/list/delete + health |

#### P1 — 重要功能（MVP 后尽快补齐）

| # | 需求 | 状态 | 说明 |
|---|------|------|------|
| 16 | Drogon 构建集成 | ✅ Done | vcpkg 已安装 Drogon，`FILEAGENT_ENABLE_DROGON=ON` 已启用 |
| 17 | LRU 缓存 | ✅ Done | 线程安全 LRU 缓存模板 + CacheManager 单例，集成到 FileService 和 SessionDao |
| 18 | Redis 缓存 | ✅ Done | hiredis 客户端 + 三组 Redis 缓存实现，配置切换 + 自动降级 LRU |
| 19 | 文件去重 | ✅ Done | 上传 API 自动 SHA-256 去重，重复文件返回已有记录 |
| 20 | 大文件分片上传 | ✅ Done | ChunkUploadService + API: init/upload/complete/status |
| 21 | 分享链接 | ✅ Done | ShareDao + ShareService + API: create/access/list/delete，支持过期 |
| 22 | 用户状态管理 | ✅ Partial | `UserDao::updateStatus` 已实现，需 API 和 admin 接口 |

#### P2 — 增强功能

| # | 需求 | 状态 | 说明 |
|---|------|------|------|
| 23 | LLM 集成 (llama.cpp/Ollama) | ✅ Done | 复用 agents.cpp-main 的 createLLM() 工厂，支持 ollama/openai/anthropic/google 四种 provider |
| 24 | 智能文件分析（LLM 驱动态） | ❌ **TODO** | 基于 LLM 的文件内容摘要/标签/分类 |
| 25 | 全文搜索 | ❌ **TODO** | 文件名 + 内容索引 |
| 26 | 单元测试 | ❌ **TODO** | 无测试框架，无测试用例 |
| 27 | Docker 容器化 | ❌ **TODO** | Dockerfile + docker-compose |
| 28 | Rate limiting / API 安全 | ❌ **TODO** | 防暴力破解 / Dos 防护 |
| 29 | Prepared Statement 迁移 | ❌ **TODO** | 当前用 `escape()` 拼接 SQL，应改为参数化查询 |
| 30 | 健康检查增强 | ✅ Done | `/health` 返回 JSON：database(available/active/status) + cache(type/initialized) |

---

## 项目进度

### 当前状态：MVP P0 全部完成

```
基础设施 ██████████████████ 100%
  ├─ CMake + vcpkg 构建     ████████████ 100%
  ├─ YAML 配置系统           ████████████ 100%
  ├─ Logger 日志            ████████████ 100%
  └─ Drogon HTTP 集成        ████████████ 100% (已启用)

数据库层 ██████████████████ 100%
  ├─ MysqlConn 封装          ████████████ 100%
  ├─ ConnectionPool 连接池   ████████████ 100%
  └─ SQL 建表 DDL            ████████████ 100%

DAO 层   ██████████████████ 100%
  ├─ UserDao                ████████████ 100%
  ├─ FileDao                ████████████ 100%
  └─ SessionDao              ████████████ 100%

服务层   ██████████████████ 100%
  ├─ AuthService (bcrypt)    ████████████ 100%
  ├─ FileService             ████████████ 100%
  └─ Session 认证            ████████████ 100%

HTTP API ██████████████████ 100%
  ├─ POST /api/user/register  ████████████ 100%
  ├─ POST /api/user/login     ████████████ 100%
  ├─ POST /api/user/logout    ████████████ 100%
  ├─ POST /api/files/upload   ████████████ 100%
  ├─ GET  /api/files          ████████████ 100%
  ├─ GET  /api/files/{id}     ████████████ 100%
  ├─ DELETE /api/files/{id}   ████████████ 100%
  ├─ GET  /health             ████████████ 100%
  ├── 分片上传 4 个路由        ████████████ 100%
  ├── 分享链接 4 个路由        ████████████ 100%
  └─ 缓存层 LRU/Redis         ████████████ 100%
```

### 关键待办（按优先级排序）

#### P0-P1 已全部完成 ✅
- 建表 DDL、密码哈希 (bcrypt)、Session 认证、全部 HTTP API 路由
- Drogon 集成、LRU/Redis 缓存、文件去重、分片上传、分享链接
- 代码质量清理：trimCopy 去重、JSON 转义、健康检查增强、认证代码抽取

#### 待推进

| # | 需求 | 优先级 | 说明 |
|---|------|--------|------|
| 1 | Docker 容器化 | **高** | Dockerfile + docker-compose，MySQL + 服务一键部署 |
| 2 | 用户状态管理 API | **中** | Admin 接口：禁用/启用用户 |
| 3 | 单元测试 | **中** | Google Test 框架，DAO/Service 层测试 |
| 4 | Rate limiting / API 安全 | **中** | 防暴力破解 / DoS 防护 |
| 5 | Prepared Statement 迁移 | **低** | `escape()` → 参数化查询 |
| 6 | 全文搜索 | **低** | 文件名 + 内容索引 |
| 7 | 智能文件分析（LLM 驱动态） | **低** | 基于 LLM 的摘要/标签/分类 |

---

## 架构说明

```
config.yaml → AppConfig → ConnectionPool.init()
                              │
    AuthMiddleware (global) ─→ 所有 /api/* 路由（Bearer <token> 鉴权）
         │
         ├── POST /api/user/register (public)
         ├── POST /api/user/login    (public) → AuthService → UserDao → bcrypt
         ├── POST /api/user/logout   → SessionDao.deleteByToken
         │
         ├── POST /api/files/upload  → FileService.createFile + SHA-256 去重 + 磁盘写入
         ├── GET  /api/files         → FileService.listUserFiles (分页)
         ├── GET  /api/files/{id}    → FileService.getFileById + stream
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
         └── GET  /health           (public)
```

### 目录结构

```
├── CMakeLists.txt          构建配置
├── config.yaml             运行配置
├── sql/
│   └── init.sql            数据库初始化（users/files/file_chunks/shares/sessions 五张表）
├── src/
│   ├── main.cpp            入口：加载配置、初始化池、注册路由、启动 HTTP
│   ├── common/
│   │   ├── Config.h        AppConfig 及其子结构体定义
│   │   ├── ConfigLoader.h  加载器声明
│   │   ├── ConfigLoader.cpp YAML 解析 + 校验 + toString
│   │   ├── Hash.h/cpp      bcrypt 密码哈希 + 安全令牌生成
│   │   ├── Logger.h        内联日志函数 (INFO/ERROR/WARN/DEBUG)
│   │   ├── StringUtil.h    字符串工具（trimCopy / jsonEscape）
│   │   ├── CacheInterface.h 缓存抽象接口
│   │   ├── CacheManager.h/cpp 缓存管理器（LRU/Redis 切换）
│   │   ├── LruCache.h      线程安全 LRU 缓存模板
│   │   ├── RedisCache.h/cpp Redis 客户端 + 三组缓存实现
│   │   └── Types.h         ApiResponse 基础类型
│   ├── db/
│   │   ├── MysqlConn.h/cpp MySQL C API 封装（连接/查询/事务/转义/存活追踪）
│   │   └── ConnectionPool.h/cpp 线程安全连接池（生产者/回收者/RAII 守卫）
│   ├── dao/
│   │   ├── Models.h        UserRecord / SessionRecord / ShareRecord / FileRecord
│   │   ├── UserDao.h/cpp   用户数据访问
│   │   ├── FileDao.h/cpp   文件数据访问
│   │   ├── SessionDao.h/cpp 会话数据访问
│   │   └── ShareDao.h/cpp  分享链接数据访问
│   ├── service/
│   │   ├── AuthService.h/cpp       用户注册/登录/状态管理
│   │   ├── FileService.h/cpp       文件 CRUD 业务逻辑
│   │   ├── ChunkUploadService.h/cpp 分片上传会话管理
│   │   ├── ShareService.h/cpp      分享链接业务逻辑
│   │   └── ServiceTypes.h          通用返回类型 (ServiceStatus/Result/ListResult)
│   └── server/
│       └── HttpServer.h    Drogon 框架头文件引用 + 说明
└── uploads/                文件存储根目录
```

---

## 技术选型

| 层面 | 选型 | 说明 |
|------|------|------|
| 语言 | C++20 | 协程、 concepts、智能指针 |
| HTTP 框架 | Drogon | epoll ET + 非阻塞 IO + C++20 协程 |
| 数据库 | MySQL 8.0+ | utf8mb4 字符集 |
| 连接池 | 自实现 | 生产者-消费者模型，RAII 守卫 |
| 缓存 | LRU→Redis | 先 LRU 实现，后续 Redis |
| LLM | llama.cpp | GGUF 模型推理 |
| 构建 | CMake 3.20+ | vcpkg 管理依赖 |
| 配置 | YAML | yaml-cpp 解析 |

---

## 代码约定

### 代码风格
- 命名空间: `fileagent`（小写）
- 类: PascalCase（`AuthService`）
- 函数/变量: camelCase（`getConnection`）
- 成员变量: snake_case_ 后缀下划线（`connection_`）
- 文件命名: PascalCase（`ConnectionPool.h`）

### 代码质量注意
- ✅ `trimCopy` 已抽取到 `src/common/StringUtil.h`（含 `jsonEscape` 工具函数）
- ✅ 认证/验证代码已抽取为 `requireAuth` / `requireJsonBody` 辅助函数
- ✅ `makeJsonResponse` 已使用 `jsonEscape` 防止 JSON 注入
- SQL 使用 `escape()` 防注入，后续应迁移到 prepared statement
- 当前无异常安全保证（`ConfigLoader.cpp` 的 `try-catch` 除外），连接池的 `shutdown()` 是唯一清理入口
- 所有 DAO 方法都从连接池取连接，没有事务协调能力——需要引入跨 DAO 事务支持

---

## 构建与运行

```bash
# 配置（启用 Drogon 需先通过 vcpkg 安装）
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake

# 构建
cmake --build build

# 运行
./build/fileagent-server [config.yaml]
```

### 依赖
- yaml-cpp（vcpkg: `yaml-cpp`）
- MySQL 客户端库（系统安装: `libmysqlclient-dev`）
- Drogon（vcpkg: `drogon`，可选 — 缺失时以 stub 模式运行）

---

## 部署备忘

- 生产需创建 MySQL database `fileagent` + 用户 `fileagent`
- `config.yaml` 中的密码/密钥不应提交到版本控制
- `uploads/` 目录需确保运行时有写入权限
- LLM 模型文件需要单独下载放入配置路径
