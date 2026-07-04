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
| 16 | Drogon 构建集成 | ⚠️ 依赖 | 需要 vcpkg 安装 Drogon，当前 `FILEAGENT_ENABLE_DROGON=OFF` 降级 stub |
| 17 | LRU 缓存 | ✅ Done | 线程安全 LRU 缓存模板 + CacheManager 单例，集成到 FileService 和 SessionDao |
| 18 | Redis 缓存 | ✅ Done | hiredis 客户端 + 三组 Redis 缓存实现，配置切换 + 自动降级 LRU |
| 19 | 文件去重 | ❌ **TODO** | `FileService::createFile` 已按哈希去重逻辑，需配合上传完整实现 |
| 20 | 大文件分片上传 | ❌ **TODO** | 需 `file_chunks` 表 + 合并逻辑 |
| 21 | 分享链接 | ❌ **TODO** | 需 `shares` 表 + 过期/访问控制 |
| 22 | 用户状态管理 | ✅ Partial | `UserDao::updateStatus` 已实现，需 API 和 admin 接口 |

#### P2 — 增强功能

| # | 需求 | 状态 | 说明 |
|---|------|------|------|
| 23 | LLM 集成 (llama.cpp) | ❌ **TODO** | 配置文件有 `model_path` / `num_threads` / `context_window`，无实现 |
| 24 | 智能文件分析（LLM 驱动态） | ❌ **TODO** | 基于 LLM 的文件内容摘要/标签/分类 |
| 25 | 全文搜索 | ❌ **TODO** | 文件名 + 内容索引 |
| 26 | 单元测试 | ❌ **TODO** | 无测试框架，无测试用例 |
| 27 | Docker 容器化 | ❌ **TODO** | Dockerfile + docker-compose |
| 28 | Rate limiting / API 安全 | ❌ **TODO** | 防暴力破解 / Dos 防护 |
| 29 | Prepared Statement 迁移 | ❌ **TODO** | 当前用 `escape()` 拼接 SQL，应改为参数化查询 |
| 30 | 健康检查增强 | ✅ Partial | 仅 `/health` 返回 "ok"，需增加数据库/缓存探活 |

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
  └─ GET  /health             ████████████ 100%
```

### 关键待办（按优先级排序）

#### P0 已全部完成 ✅
- 建表 DDL、密码哈希 (bcrypt)、Session 认证、全部 8 个 HTTP API 路由

#### P1 待推进

| # | 需求 | 优先级 | 说明 |
|---|------|--------|------|
| 1 | 缓存实现 | **高** | LRU 缓存（第一优先），Redis 实现（后续） |
| 2 | 文件去重优化 | **高** | 当前已按哈希去重，需完善上传体验 |
| 3 | 大文件分片上传 | **中** | 需 `file_chunks` 表 + 合并逻辑 | 
| 4 | 分享链接 | **中** | 需 `shares` 表 + 过期/访问控制 |
| 5 | 用户状态管理 API | **中** | `updateStatus` 已实现，缺 admin 接口 |

#### P2 待推进

| # | 需求 | 说明 |
|---|------|------|
| 1 | LLM 集成 (llama.cpp) | 配置文件已就绪，无实现 |
| 2 | 智能文件分析 | LLM 驱动态摘要/标签/分类 |
| 3 | 全文搜索 | 文件名 + 内容索引 |
| 4 | 单元测试 | 无测试框架，无测试用例 |
| 5 | Docker 容器化 | Dockerfile + docker-compose |
| 6 | Rate limiting / API 安全 | 防暴力破解 / DoS 防护 |
| 7 | Prepared Statement 迁移 | `escape()` → 参数化查询 |
| 8 | 健康检查增强 | 增加数据库/缓存探活 |
| 9 | `trimCopy` 去重 | AuthService/FileService 中重复定义，需抽离到公共工具 |

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
│   │   └── Types.h         ApiResponse 基础类型
│   ├── db/
│   │   ├── MysqlConn.h/cpp MySQL C API 封装（连接/查询/事务/转义/存活追踪）
│   │   └── ConnectionPool.h/cpp 线程安全连接池（生产者/回收者/RAII 守卫）
│   ├── dao/
│   │   ├── Models.h        UserRecord / FileRecord / SessionRecord 数据模型
│   │   ├── UserDao.h/cpp   用户数据访问
│   │   ├── FileDao.h/cpp   文件数据访问
│   │   └── SessionDao.h/cpp 会话数据访问
│   ├── service/
│   │   ├── AuthService.h/cpp   用户注册/登录/状态管理
│   │   ├── FileService.h/cpp   文件 CRUD 业务逻辑
│   │   └── ServiceTypes.h      通用返回类型 (ServiceStatus/Result/ListResult)
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
- `trimCopy` 在 `AuthService.cpp` 和 `FileService.cpp` 中有重复定义，需抽取公共工具函数
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
