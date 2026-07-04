# FileAgent Server — C++ AI 智能文件管理服务

## 项目需求文档 v1.0

---

## 1. 项目概述

### 1.1 项目定位

**FileAgent Server** 是一个基于 **C++20** 的高性能 RESTful 文件管理后端服务，内置 **AI Agent 引擎**，支持用户通过自然语言对文件进行语义检索和智能管理。本项目综合运用了 Linux 网络编程、数据库连接池、分布式缓存、大模型推理、RAG 检索、Agent 编排等技术栈，是一个面向 C++ 后端开发岗位的综合型项目。

### 1.2 核心功能

| 功能模块 | 功能点 | 说明 |
|---------|--------|------|
| HTTP 服务 | RESTful API | 基于 Drogon（epoll 框架）构建 HTTP Server |
| 用户管理 | 注册 / 登录 / 个人信息 | Session + Token 认证 |
| 文件管理 | 上传 / 下载 / 删除 / 列表 | 支持秒传与分片上传 |
| 文件分享 | 分享 / 转存 / 取消分享 | 分享码 + 排行榜 |
| AI Agent | 自然语言对话式操作 | ReAct Agent Loop |
| 语义搜索 | 文件内容级别语义检索 | RAG 管道 |
| 容器化部署 | Docker + docker-compose | 多阶段构建 |

### 1.3 技术栈

| 层次 | 技术 | 说明 |
|------|------|------|
| 语言 | **C++20** | 智能指针、RAII、线程库、optional/variant |
| 网络 | **Linux epoll**（ET 模式） | 事件驱动 + 非阻塞 IO |
| HTTP | **Drogon 框架（epoll 驱动）** | 路由 / 中间件 / WebSocket / SSE |
| 数据库 | **MySQL**（C API - libmysqlclient） | 关系型数据持久化 |
| 连接池 | **自实现 MySQL 连接池** | 单例 + RAII + 生产者消费者模型 |
| 缓存 | **Redis**（hiredis）或自实现 LRU Cache | Session + 热点数据加速 |
| 序列化 | **nlohmann/json** | JSON 序列化/反序列化 |
| AI 推理 | **llama.cpp** | C++ 原生本地推理 + embedding |
| 向量检索 | **自实现**（余弦相似度） | 理解向量检索原理 |
| Agent | **自实现 ReAct 模式** | Thought→Action→Observation |
| 构建 | **CMake + vcpkg** | 现代 C++ 构建管理 |
| 部署 | **Docker** 多阶段构建 + docker-compose | 容器化部署 |

---

## 2. 系统架构

### 2.1 整体架构

```
┌──────────────────────────────────────────────────────────────────┐
│                        FileAgent Server                           │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌────────────────────────────────────────────────────┐          │
│  │            HTTP Server（Drogon 框架）                 │          │
│  │                                                    │          │
│  │  ┌──────────┐  ┌──────────┐  ┌─────────────────┐  │          │
│  │  │ epoll    │  │ 路由     │  │ 中间件链         │  │          │
│  │  │ 事件循环  │  │ Router   │  │ (认证/日志/限流) │  │          │
│  │  └──────────┘  └──────────┘  └─────────────────┘  │          │
│  └──────────────────┬─────────────────────────────────┘          │
│                     │                                           │
│  ┌──────────────────▼─────────────────────────────────┐          │
│  │                 Controller 层                        │          │
│  │  ┌────────┐ ┌────────┐ ┌────────┐ ┌─────────────┐ │          │
│  │  │ User   │ │ File   │ │ Share  │ │ Agent       │ │          │
│  │  │Ctrl    │ │Ctrl    │ │Ctrl    │ │Ctrl (新建)   │ │          │
│  │  └───┬────┘ └───┬────┘ └───┬────┘ └──────┬──────┘ │          │
│  └──────┼──────────┼──────────┼──────────────┼────────┘          │
│         │          │          │              │                   │
│  ┌──────▼──────────▼──────────▼──────────────▼────────┐          │
│  │                  Service 层                         │          │
│  │  业务逻辑编排（事务、缓存策略、Agent 调度）           │          │
│  └──────┬──────────┬──────────┬──────────────┬────────┘          │
│         │          │          │              │                   │
│  ┌──────▼──────────▼──────────▼──────────────▼────────┐          │
│  │                   DAO 层                            │          │
│  │  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────────┐  │          │
│  │  │UserDAO │ │FileDAO │ │ShareDAO│ │ChunkDAO    │  │          │
│  │  └───┬────┘ └───┬────┘ └───┬────┘ └─────┬──────┘  │          │
│  └──────┼──────────┼──────────┼──────────────┼────────┘          │
│         │          │          │              │                   │
│  ┌──────▼──────────▼──────────▼──────────────▼────────┐          │
│  │              基础设施层                              │          │
│  │                                                     │          │
│  │  ┌──────────────────────┐                           │          │
│  │  │   MySQL 连接池 ★      │  ← 核心组件                 │          │
│  │  └──────────────────────┘                           │          │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────────────┐  │          │
│  │  │ MySQL   │  │ Redis /  │  │  llama.cpp       │  │          │
│  │  │ 数据库   │  │ LRU Cache│  │ 推理 + Embedding │  │          │
│  │  └──────────┘  └──────────┘  └──────────────────┘  │          │
│  └─────────────────────────────────────────────────────┘          │
│                                                                  │
│  ┌─────────────────────────────────────────────────────┐          │
│  │          CLI 客户端                                   │          │
│  │  命令行交互式 Agent 对话 / 直接 API 调用              │          │
│  └─────────────────────────────────────────────────────┘          │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### 2.2 模块职责划分

| 层级 | 模块 | 职责 |
|------|------|------|
| 网络层 | **HTTPServer** | epoll 事件循环、TCP 连接管理、HTTP 请求解析、响应构建 |
| 路由层 | **Router** | URL 路径匹配、HTTP 方法与 Controller 绑定、参数提取 |
| 中间件 | **Middleware** | 认证中间件（Session/Token 校验）、日志中间件、限流中间件 |
| 控制层 | **Controller** | 参数校验、调用 Service、构建 HTTP 响应 |
| 业务层 | **Service** | 业务逻辑编排、事务管理、缓存策略、Agent 调度 |
| 数据层 | **DAO** | SQL 拼装、数据库读写、结果集映射 |
| 基础设施 | **ConnectionPool** | 连接复用、心跳保活、自动扩缩 |
| AI 引擎 | **AgentEngine** | ReAct Loop、工具注册与调用、RAG 管道编排 |
| AI 推理 | **LLMClient** | llama.cpp 推理接口封装、embedding 接口封装 |
| 检索 | **VectorStore** | 向量索引构建、余弦相似度计算、Top-K 检索 |

---

## 3. 详细模块设计

### 3.1 HTTP Server 模块

> **方案选择说明**：原始方案考虑手写 epoll + HTTP 状态机解析，但协议细节（chunked 编码、pipelining、Keep-Alive 超时管理）工作量较大且并非项目核心竞争力。推荐以下 **务实路线**：

#### 方案一（推荐）：Drogon 框架 + 手写核心组件

**Drogon**（[github.com/drogonframework/drogon](https://github.com/drogonframework/drogon)）是一个基于 epoll 的 C++17/20 HTTP 框架：
- 底层正是 **epoll ET + 非阻塞 IO + 多线程 Reactor** 模型 — 与你原始设计完全一致
- 支持协程（C++20 Coroutines）、WebSocket、SSE 流式输出
- 内置 JSON、路由、中间件链、静态文件服务
- 国产开源，中文文档完善，社区活跃

**采用 Drogon 后，项目的核心竞争力不受影响**，你仍然需要手写：
- MySQL 连接池（★ 核心）
- DAO + 事务管理
- AI Agent 引擎（★ 核心）
- RAG 管道 + 向量检索（★ 核心）
- llama.cpp 推理封装
- LRU Cache / 缓存策略

**如果面试官问 HTTP Server 实现**，仍然可以展开回答 epoll ET/Reactor 模型/非阻塞 IO — Drogon 就是在这些原理上构建的，了解框架底层就是了解这些概念。

#### 方案二（折中）：自实现 epoll + llhttp 解析器

如果希望保留更多"手写"成分但跳过最繁琐的部分：
- **自实现 epoll 事件循环 + 线程池**（核心学习价值，保留）
- **HTTP 解析改用 llhttp**（[github.com/nodejs/llhttp](https://github.com/nodejs/llhttp)）— Node.js 使用的 HTTP 解析器，纯 C 状态机，2000 行，经过生产验证
- 这样你保留了网络框架层面的手写，把**解析协议这种脏活**交给专业库

#### 3.1.1 功能需求（以 Drogon 方案为例）

- 基于 **epoll ET 模式 + 非阻塞 IO**（Drogon 底层实现）
- 支持多线程 + 协程（Drogon 默认支持）
- HTTP/1.1 协议解析（Drogon 内置）
- 支持 GET、POST、PUT、DELETE 方法
- 支持 URL 路径参数解析
- 支持 chunked 传输编码
- 中间件链（认证 / 日志 / 限流）
- WebSocket / SSE 流式支持（Agent 对话需要）

#### 3.1.2 核心接口设计（Drogon 风格）

```cpp
// 使用 Drogon 的接口风格
int main() {
    using namespace drogon;
    
    // 注册路由
    app().registerHandler("/api/user/register",
        [](const HttpRequestPtr& req, 
           std::function<void(const HttpResponsePtr&)> callback) {
            // 处理注册逻辑
            auto resp = HttpResponse::newHttpJsonResponse(
                {{"code", 0}, {"message", "success"}});
            callback(resp);
        },
        {Post});
    
    app().registerHandler("/api/agent/chat",
        [](const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)> callback) {
            // Agent SSE 流式响应
            auto resp = HttpResponse::newAsyncResponse();
            // ... Agent 逻辑
        },
        {Post});
    
    // 注册中间件
    app().registerMiddleware(AuthMiddleware());
    app().registerMiddleware(RateLimitMiddleware());
    
    // 启动
    app().setLogPath("./logs")
         .setLogLevel(trantor::Logger::kInfo)
         .addListener("0.0.0.0", 10086)
         .setThreadNum(4)
         .run();
}
```

#### 3.1.3 关键技术点

- Drogon 底层模型：epoll ET + 非阻塞 IO + 多线程 Reactor
- 协程支持：简化异步回调，天然支持 SSE 流式输出
- 中间件链：认证、限流、CORS
- 文件上传：内置 multipart 解析

---

### 3.2 MySQL 连接池模块（★ 核心）

#### 3.2.1 功能需求

- 单例模式管理全局连接池
- 支持初始化时创建最小连接数
- RAII 封装：`ConnectionGuard` 自动获取/归还连接
- 空闲连接心跳保活
- 连接超时检测与自动回收
- 最大连接数限制，超出时阻塞等待或超时返回
- 线程安全（`std::mutex` + `std::condition_variable`）

#### 3.2.2 核心接口设计

```cpp
class ConnectionPool {
public:
    static ConnectionPool& getInstance(); // 单例

    struct Config {
        std::string host = "127.0.0.1";
        uint16_t port = 3306;
        std::string user;
        std::string password;
        std::string database;
        size_t min_size = 4;        // 最小连接数
        size_t max_size = 20;       // 最大连接数
        std::chrono::seconds timeout{5};       // 获取连接超时
        std::chrono::seconds idle_timeout{300};// 空闲连接超时
        std::chrono::seconds heartbeat_interval{60}; // 心跳间隔
    };

    class ConnectionGuard {
    public:
        ConnectionGuard(ConnectionPool& pool, MYSQL* conn);
        ~ConnectionGuard();  // 自动归还连接
        MYSQL* get();
        MYSQL* operator->();
    };

    void init(const Config& config);
    ConnectionGuard getConnection();
    
    // 事务支持：获取 / 归还原始连接（供 TransactionGuard 使用）
    MYSQL* allocateConnection();
    void returnConnection(MYSQL* conn);
    
    size_t available() const;  // 当前可用连接数
    size_t active() const;     // 当前活跃连接数
    void shutdown();

private:
    std::queue<MYSQL*> available_;              // 可用连接队列
    std::mutex mutex_;                          // 互斥锁
    std::condition_variable cv_;                // 条件变量
    std::atomic<size_t> active_count_{0};       // 活跃连接数
    Config config_;                             // 配置
    std::thread health_check_thread_;           // 心跳线程
    std::atomic<bool> running_{true};
};
```

#### 3.2.3 连接生命周期

```
                 ┌─────────────────────┐
                 │   init(config)       │
                 │  创建 min_size 个连接  │
                 └─────────┬───────────┘
                           │
              ┌────────────▼────────────┐
              │   连接池（连接队列）       │
              │   available_            │
              └────────────┬────────────┘
                           │
              ┌────────────▼────────────┐
              │   getConnection()       │
              │  从队列取出 / 创建新连接   │
              └────────────┬────────────┘
                           │
              ┌────────────▼────────────┐
              │   ConnectionGuard       │
              │  （RAII 使用）           │
              │  → 查询 / 写入           │
              └────────────┬────────────┘
                           │
              ┌────────────▼────────────┐
              │  ~ConnectionGuard()     │
              │  归还到 available_      │
              │  通知等待者              │
              └─────────────────────────┘
```

#### 3.2.4 心跳保活

> **优化要点**：避免在持有锁时执行 IO 操作（`mysql_ping`），先拷贝连接指针列表到局部变量，尽早释放互斥锁，在无锁状态下执行网络 IO。

```cpp
void healthCheckLoop() {
    while (running_) {
        std::this_thread::sleep_for(config_.heartbeat_interval);
        
        // ① 加锁快速取出所有待检查连接
        std::vector<MYSQL*> to_check;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            size_t current = available_.size();
            to_check.reserve(current);
            for (size_t i = 0; i < current; ++i) {
                to_check.push_back(available_.front());
                available_.pop();
            }
        }  // ② 尽早释放锁 —— 互斥锁不跨越 IO 操作
        
        // ③ 在无锁状态下执行 IO 操作（mysql_ping 是网络调用）
        std::vector<MYSQL*> alive;
        for (MYSQL* conn : to_check) {
            if (mysql_ping(conn) != 0) {
                mysql_close(conn);
                conn = createNewConnection();
            }
            if (conn) alive.push_back(conn);
        }
        
        // ④ 重新加锁归还活着的连接
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (MYSQL* conn : alive) {
                available_.push(conn);
            }
            cv_.notify_all();  // 通知等待的线程
            
            // ⑤ 缩容：空闲连接超过 min_size 时，释放多余的连接
            //    （仅在低峰期执行，释放最近最少使用的连接）
            if (alive.size() > config_.min_size) {
                size_t excess = alive.size() - config_.min_size;
                for (size_t i = 0; i < excess; ++i) {
                    mysql_close(alive.back());
                    alive.pop_back();
                }
            }
        }
    }
}
```

> **缩容策略**：每次心跳检查时，如果可用连接数超过 `min_size`，释放多余的连接，避免高峰期创建的连接在低谷期仍占用数据库资源。

---

### 3.3 DAO 数据访问层

#### 3.3.1 功能需求

- 每个数据库表有对应的 DAO 类
- 封装 SQL 语句，对外提供类型安全的 API
- 通过连接池获取连接
- 结果集到 C++ 对象的映射
- **支持跨 DAO 事务**：通过 `TransactionGuard` 确保多个 DAO 操作在同一个数据库连接中执行

#### 3.3.2 事务管理（TransactionGuard）

> **优化说明**：原始设计每个 DAO 各自获取连接，无法保证跨 DAO 操作的事务一致性。引入 `TransactionGuard`，在 Service 层开启事务，统一管理连接的获取、事务提交/回滚。

```cpp
class TransactionGuard {
public:
    // 从连接池获取一个连接并开启事务
    TransactionGuard(ConnectionPool& pool);
    ~TransactionGuard();

    // 禁止拷贝
    TransactionGuard(const TransactionGuard&) = delete;
    TransactionGuard& operator=(const TransactionGuard&) = delete;

    // 允许移动（转移连接所有权）
    TransactionGuard(TransactionGuard&& other) noexcept;
    TransactionGuard& operator=(TransactionGuard&& other) noexcept;

    void commit();    // 提交事务
    void rollback();  // 回滚事务

    MYSQL* get();              // 获取原生连接
    MYSQL* operator->();       // 便捷访问

private:
    ConnectionPool* pool_;
    MYSQL* conn_;
    bool active_ = false;      // 事务是否活跃
};

// 使用示例（在 Service 层）：
// {
//     TransactionGuard tx(pool);
//     userDao.insert(user, tx.get());     // 传事务连接
//     fileDao.insert(file, tx.get());     // 同一连接
//     tx.commit();                         // 一起提交
// }  // 析构时若未 commit 则自动 rollback
```

> **扩展思路**：每个 DAO 方法提供两个重载 — `操作(ConnectionPool&)` 单次调用自己取连接，`操作(MYSQL*)` 参与已开启的事务。

#### 3.3.3 核心接口设计

```cpp
class UserDAO {
public:
    explicit UserDAO(ConnectionPool& pool);
    
    // 自动获取连接的方式（单次操作）
    std::optional<User> findByUsername(const std::string& username);
    std::optional<User> findById(int64_t id);
    bool insert(const User& user);
    bool updatePassword(int64_t id, const std::string& password_hash);
    bool updateLoginTime(int64_t id);
    
    // 参与事务的方式（接收外部连接，由 TransactionGuard 管理）
    // 后缀 WithTx 便于区分，也可用重载
    std::optional<User> findByUsername(const std::string& username, MYSQL* conn);
    std::optional<User> findById(int64_t id, MYSQL* conn);
    bool insert(const User& user, MYSQL* conn);
    bool updatePassword(int64_t id, const std::string& password_hash, MYSQL* conn);
};

class FileDAO {
public:
    explicit FileDAO(ConnectionPool& pool);
    
    std::optional<FileInfo> findById(int64_t id);
    std::optional<FileInfo> findByHash(const std::string& hash);
    std::vector<FileInfo> findByUserId(int64_t user_id, int page, int size);
    bool insert(const FileInfo& file);
    bool updateStatus(int64_t id, int status);
    bool deleteById(int64_t id);  // 逻辑删除
    int64_t countByUserId(int64_t user_id);
};

class ShareDAO {
public:
    explicit ShareDAO(ConnectionPool& pool);
    
    bool insert(const ShareInfo& share);
    std::optional<ShareInfo> findByCode(const std::string& code);
    bool incrementViewCount(int64_t id);
    bool expireShare(int64_t id);
    std::vector<ShareInfo> findByOwner(int64_t owner_id);
    // Redis 排行榜
    void incrementRank(const std::string& share_code);
    std::vector<ShareInfo> getTopShares(int limit);
};

class ChunkDAO {
public:
    explicit ChunkDAO(ConnectionPool& pool);
    
    bool insert(const FileChunk& chunk);
    std::vector<FileChunk> findByFileId(int64_t file_id);
    int countByFileId(int64_t file_id);
    bool deleteByFileId(int64_t file_id);
    bool updateStatus(int64_t id, int status);
};
```

---

### 3.4 用户与文件管理模块

#### 3.4.1 用户管理

- 用户注册：用户名、密码（bcrypt 哈希 + salt）、邮箱
- 用户登录：密码验证 → 生成 Session Token（Redis 或内存缓存）
- Session 生命周期管理：过期时间、主动登出

#### 3.4.2 文件秒传

```
客户端上传流程：
  1. 客户端计算文件 SHA-256
  2. 客户端发送 SHA-256 到服务端校验
  3. 服务端查 FileDAO.findByHash(hash)
     → 存在：返回"文件已存在"，客户端直接引用
     → 不存在：返回"需要上传"，开始上传流程
```

#### 3.4.3 大文件分片上传

```
分片上传流程：
  1. 客户端：文件分割为 N 个分片（每片 4MB）
  2. 客户端上传每个分片（含文件 ID + 分片序号 + 分片 SHA-256）
  3. 服务端：每收到一个分片 → 写入临时目录 → ChunkDAO 记录
  4. 所有分片上传完成后，客户端发送合并请求
  5. 服务端：按分片序号顺序合并文件
  6. 合并完成 → 文件从临时目录移动到正式存储目录
  7. 更新 FileDAO 状态：临时→正常
  
异常处理：
  - 分片上传超时：清理已上传的分片
  - 分片 SHA-256 校验失败：返回重传
  - 合并过程中断：回滚，保留分片允许重新合并
```

#### 3.4.4 文件分享

- 生成唯一分享码（UUID 或哈希截取）
- 支持设置过期时间
- 分享计数 + Redis 排行榜（热门分享 Top-N）
- 转存文件：将分享文件复制到目标用户目录

---

### 3.5 AI Agent 引擎模块（★ 核心）

#### 3.5.1 Agent Loop 设计（ReAct 模式）

```
入口：用户输入自然语言查询/指令

Agent Loop 循环：
  Step 1 — 思考（Thought）
    分析用户意图，决定下一步行动
    LLM 输出："我需要搜索用户文件来找到相关信息"
    
  Step 2 — 行动（Action）
    选择一个工具并构造参数
    输出格式：{"tool": "search_files_by_semantic", "args": {"query": "..."}}
    
  Step 3 — 观察（Observation）
    执行工具，获取结果
    将结果注入到 LLM 上下文
    
  Step 4 — 重复，直到 LLM 给出最终回答（Final Answer）
  或达到最大迭代次数（如 10 次）后强制结束
```

#### 3.5.2 工具系统

| 工具名称 | 功能 | 参数 | 对应操作 |
|---------|------|------|---------|
| `search_files_by_semantic` | 语义搜索文件 | query: string, limit: int | RAG 检索 |
| `search_files_by_keyword` | 关键词搜索文件 | keyword: string | SQL LIKE 查询 |
| `get_file_info` | 查看文件详情 | file_id: int | FileDAO.findById |
| `list_files` | 列出目录文件 | path: string, page: int | FileDAO.findByUserId |
| `summarize_file` | 总结文件内容 | file_id: int | 读取文件 + LLM 总结 |
| `organize_files_by_type` | 按类型整理文件 | directory: string | 移动文件操作 |
| `delete_file` | 删除文件（需确认） | file_id: int | FileDAO.deleteById |

```cpp
// 工具注册
class ToolRegistry {
public:
    using ToolFn = std::function<std::string(const nlohmann::json& args)>;
    
    struct Tool {
        std::string name;
        std::string description;
        nlohmann::json parameters;  // JSON Schema
        ToolFn execute;
    };
    
    void registerTool(const Tool& tool);
    std::optional<Tool> find(const std::string& name);
    std::string getToolsDescription();  // 为 LLM 生成工具描述
};
```

#### 3.5.3 Agent 引擎核心

```cpp
class AgentEngine {
public:
    struct Config {
        int max_steps = 10;                // 最大推理步数
        std::chrono::seconds timeout{60};  // 超时
        std::string system_prompt;         // 系统提示词
    };
    
    struct Result {
        bool success;
        std::string answer;     // 最终回答
        int steps_used;         // 实际步数
        std::vector<std::string> tools_used;
        std::string error;      // 错误信息
    };

    Result process(const std::string& user_input, 
                   const std::string& session_id);
    
private:
    ToolRegistry tools_;
    std::shared_ptr<LLMClient> llm_;
    Config config_;
};
```

#### 3.5.4 错误恢复策略

- 工具执行失败 → 重试 1 次 → 告诉 LLM 失败原因 → LLM 选择其他工具
- LLM 输出格式错误 → 重新解析 → 重试最多 3 次
- 超时 → 返回已收集的部分结果
- 重复行动检测 → 若连续 3 步相同工具+参数，强制结束

---

### 3.6 RAG 管道模块

#### 3.6.1 处理流程

```
用户上传文件
    │
    ├──→ [异步] 1. 文本提取  ← 文件上传接口立即返回，后台异步处理
    │        支持 txt, pdf, docx, md
    ▼
┌──────────────────────┐
│  1. 文本提取          │  ← 支持 txt, pdf, docx, md
│     提取纯文本内容     │
└─────────┬────────────┘
          │
          ▼
┌──────────────────────┐
│  2. 文档分块          │  ← 滑动窗口策略
│     按段落/大小分割    │     块大小: 512 tokens
│     保留元数据(文件名) │     重叠: 64 tokens
└─────────┬────────────┘
          │
          ▼
┌──────────────────────┐
│  3. 向量化            │  ← llama.cpp embedding API
│     BGE-M3 模型       │     输出 1024 维向量
│     存储到内存索引     │     定期持久化到磁盘
└─────────┬────────────┘
          │
          ▼
┌──────────────────────┐
│  4. 检索              │  ← 余弦相似度
│     用户查询 → 向量化  │     Top-K 检索 (K=5)
│     返回最匹配分片     │
└─────────┬────────────┘
          │
          ▼
┌──────────────────────┐
│  5. 增强生成          │  ← 注入 LLM 上下文
│     检索结果 + 用户查询│     生成回答
│     返回给用户         │
└──────────────────────┘
```

#### 3.6.2 向量存储设计

```cpp
class VectorStore {
public:
    struct Document {
        std::string id;
        std::string content;      // 文本内容
        std::string file_name;    // 来源文件名
        std::vector<float> embedding; // 向量
    };
    
    struct SearchResult {
        std::string content;
        std::string file_name;
        float score;              // 相似度分数
    };

    void addDocument(Document doc);
    void addDocuments(std::vector<Document> docs);
    void removeByFileId(const std::string& file_id);
    
    std::vector<SearchResult> search(
        const std::vector<float>& query_embedding, 
        int top_k = 5);
    
    size_t size() const;
    void clear();
    
    // 持久化（避免服务重启后全量重建索引）
    bool saveToDisk(const std::string& path);     // 序列化为 JSON / 二进制
    bool loadFromDisk(const std::string& path);   // 反序列化重建索引

private:
    struct IndexEntry {
        std::vector<float> embedding;
        std::string content;
        std::string file_name;
    };
    std::vector<IndexEntry> index_;
    std::shared_mutex rw_mutex_;
    std::string persist_path_;  // 持久化文件路径（空=不持久化）
};

// 余弦相似度计算
float cosineSimilarity(const std::vector<float>& a, 
                       const std::vector<float>& b);
```

#### 3.6.3 文档分块器

```cpp
class TextChunker {
public:
    struct Chunk {
        std::string content;
        int index;
        std::string source_file;
    };
    
    // 滑动窗口分块
    std::vector<Chunk> chunk(const std::string& text,
                             const std::string& source_file,
                             size_t chunk_size = 512,
                             size_t overlap = 64);
    
    // 按段落分块（Markdown/文本）
    std::vector<Chunk> chunkByParagraph(
        const std::string& text,
        const std::string& source_file);
};
```

---

### 3.7 llama.cpp 集成模块

#### 3.7.1 功能需求

- C++ 原生调用 llama.cpp 进行文本推理（不通过 HTTP API）
- 调用 llama.cpp 的 embedding 接口生成文本向量
- 支持流式输出回调
- 线程池管理推理请求

#### 3.7.2 核心接口

```cpp
class LLMClient {
public:
    struct Config {
        std::string model_path;        // GGUF 模型路径
        std::string embedding_model;   // Embedding 模型路径
        int n_gpu_layers = 0;          // GPU 层数 (0=CPU)
        int context_size = 4096;       // 上下文大小
        int batch_size = 512;          // 批处理大小
        float temperature = 0.7;       // 生成温度
        int max_tokens = 2048;         // 最大生成长度
    };
    
    struct CompletionResult {
        bool success;
        std::string text;
        std::vector<std::string> tool_calls;  // 解析出的工具调用
    };

    void init(const Config& config);
    CompletionResult generate(const std::string& prompt);
    std::vector<float> embed(const std::string& text);
    void setStreamCallback(std::function<void(const std::string&)> cb);
};
```

---

### 3.8 缓存模块

#### 3.8.1 缓存策略

| 数据类型 | 缓存策略 | 存储 | 过期时间 |
|---------|---------|------|---------|
| Session Token | Redis / LRU Cache | key=session_id, value=user_id | 30 分钟 |
| 用户信息 | Redis / LRU Cache | key=user:id, value=User JSON | 10 分钟 |
| 文件列表（热门） | Redis / LRU Cache | key=files:user:id, value=JSON | 5 分钟 |
| 分享排行榜 | Redis Sorted Set | key=share:ranking | 持久化 |
| Embedding 缓存 | 内存 LRU Cache | key=text_hash, value=vector | 按 LRU 淘汰 |

#### 3.8.2 LRU Cache 自实现

```cpp
template<typename K, typename V>
class LRUCache {
public:
    explicit LRUCache(size_t capacity);
    
    void put(const K& key, const V& value);
    std::optional<V> get(const K& key);
    void remove(const K& key);
    void clear();
    size_t size() const;
    
private:
    struct Node {
        K key;
        V value;
        Node* prev;
        Node* next;
    };
    
    std::unordered_map<K, Node*> map_;
    std::unique_ptr<Node> head_, tail_;  // 哨兵节点
    size_t capacity_;
    std::shared_mutex mutex_;
};
```

---

## 4. 数据库设计

### 4.1 数据库总览

```sql
-- 创建数据库
CREATE DATABASE IF NOT EXISTS fileagent 
    DEFAULT CHARACTER SET utf8mb4 
    DEFAULT COLLATE utf8mb4_unicode_ci;
```

### 4.2 用户表

```sql
CREATE TABLE users (
    id          BIGINT PRIMARY KEY AUTO_INCREMENT,
    username    VARCHAR(64) UNIQUE NOT NULL,
    password    VARCHAR(256) NOT NULL,          -- bcrypt 哈希（salt 内嵌在哈希结果中，无需单独字段）
    email       VARCHAR(128) DEFAULT '',
    avatar_url  VARCHAR(256) DEFAULT '',
    status      TINYINT DEFAULT 1,              -- 1:正常 0:禁用
    total_files INT DEFAULT 0,                  -- 文件总数
    used_space  BIGINT DEFAULT 0,               -- 已用空间(byte)
    created_at  DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at  DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    
    INDEX idx_username (username),
    INDEX idx_status (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 4.3 文件表

```sql
CREATE TABLE files (
    id          BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id     BIGINT NOT NULL,
    filename    VARCHAR(256) NOT NULL,
    file_hash   VARCHAR(64) NOT NULL,            -- SHA-256
    file_size   BIGINT NOT NULL DEFAULT 0,
    file_type   VARCHAR(32) DEFAULT '',          -- 文件类型(扩展名)
    storage_path VARCHAR(512) NOT NULL,          -- 存储路径
    is_chunked  TINYINT DEFAULT 0,              -- 是否分片 0:否 1:是
    chunk_count INT DEFAULT 0,                   -- 分片数
    status      TINYINT DEFAULT 0,               -- -1:已删除 0:临时 1:正常
    created_at  DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at  DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    
    INDEX idx_user_id (user_id),
    INDEX idx_file_hash (file_hash),
    INDEX idx_status (status),
    INDEX idx_created (created_at),
    CONSTRAINT fk_files_user FOREIGN KEY (user_id) REFERENCES users(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 4.4 文件分片表

```sql
CREATE TABLE file_chunks (
    id          BIGINT PRIMARY KEY AUTO_INCREMENT,
    file_id     BIGINT NOT NULL,
    chunk_index INT NOT NULL,                   -- 分片序号(从0开始)
    chunk_hash  VARCHAR(64) NOT NULL,            -- 分片 SHA-256
    chunk_size  INT NOT NULL,                    -- 分片大小
    storage_path VARCHAR(512) NOT NULL,          -- 分片存储路径
    status      TINYINT DEFAULT 0,               -- 0:未完成 1:已完成
    created_at  DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at  DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    
    INDEX idx_file_id (file_id),
    INDEX idx_status (status),
    CONSTRAINT fk_chunks_file FOREIGN KEY (file_id) REFERENCES files(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 4.5 分享表

```sql
CREATE TABLE shares (
    id          BIGINT PRIMARY KEY AUTO_INCREMENT,
    file_id     BIGINT NOT NULL,
    owner_id    BIGINT NOT NULL,
    share_code  VARCHAR(32) UNIQUE NOT NULL,     -- 分享码
    share_type  TINYINT DEFAULT 0,               -- 0:公开 1:私密(需要密码)
    password    VARCHAR(128) DEFAULT '',          -- 分享密码(空=无)
    view_count  INT DEFAULT 0,                   -- 浏览次数
    save_count  INT DEFAULT 0,                   -- 转存次数
    expire_at   DATETIME NULL,                   -- 过期时间(NULL=永久)
    status      TINYINT DEFAULT 1,               -- 1:有效 0:已取消
    created_at  DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    INDEX idx_share_code (share_code),
    INDEX idx_owner_id (owner_id),
    INDEX idx_status (status),
    INDEX idx_expire (expire_at),
    CONSTRAINT fk_shares_file FOREIGN KEY (file_id) REFERENCES files(id),
    CONSTRAINT fk_shares_user FOREIGN KEY (owner_id) REFERENCES users(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 4.6 Session 表（可选，如果不用 Redis）

```sql
CREATE TABLE sessions (
    id          BIGINT PRIMARY KEY AUTO_INCREMENT,
    session_id  VARCHAR(128) UNIQUE NOT NULL,    -- Token
    user_id     BIGINT NOT NULL,
    ip_address  VARCHAR(45) DEFAULT '',
    user_agent  VARCHAR(512) DEFAULT '',
    expire_at   DATETIME NOT NULL,
    created_at  DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    INDEX idx_session (session_id),
    INDEX idx_user (user_id),
    INDEX idx_expire (expire_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

---

## 5. 接口设计（RESTful API）

### 5.1 用户模块

| 方法 | 路径 | 功能 | 认证 |
|------|------|------|------|
| POST | `/api/user/register` | 用户注册 | 否 |
| POST | `/api/user/login` | 用户登录 | 否 |
| POST | `/api/user/logout` | 用户登出 | 是 |
| GET | `/api/user/info` | 获取用户信息 | 是 |

**注册请求：**
```json
POST /api/user/register
{
    "username": "test_user",
    "password": "123456",
    "email": "test@example.com"
}
```

**注册响应：**
```json
{
    "code": 0,
    "message": "success",
    "data": {
        "user_id": 1,
        "username": "test_user"
    }
}
```

**登录响应：**
```json
{
    "code": 0,
    "message": "success",
    "data": {
        "token": "session_xxxxx",
        "user": {
            "id": 1,
            "username": "test_user"
        }
    }
}
```

### 5.2 文件模块

| 方法 | 路径 | 功能 | 认证 |
|------|------|------|------|
| POST | `/api/files/upload` | 文件上传 | 是 |
| POST | `/api/files/upload/chunk` | 分片上传 | 是 |
| POST | `/api/files/upload/merge` | 合并分片 | 是 |
| POST | `/api/files/check` | SHA-256 秒传校验 | 是 |
| GET | `/api/files/list` | 文件列表 | 是 |
| GET | `/api/files/download/:id` | 文件下载 | 是 |
| DELETE | `/api/files/:id` | 删除文件 | 是 |

**文件上传（multipart/form-data）：**
```
POST /api/files/upload
Headers: Authorization: Bearer <token>
Body: file=@test.pdf
```

**SHA-256 秒传校验：**
```json
POST /api/files/check
{
    "file_hash": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "filename": "test.pdf"
}
```

**秒传校验响应：**
```json
{
    "code": 0,
    "data": {
        "exists": true,
        "file_id": 123
    }
}
// 或
{
    "code": 0,
    "data": {
        "exists": false
    }
}
```

**分片上传（使用 multipart/form-data，避免 Base64 编码开销）：**
```
POST /api/files/upload/chunk
Headers: Authorization: Bearer <token>
Body: (multipart/form-data)
  file_id: 123
  chunk_index: 0
  chunk_hash: <sha256>
  chunk_data: <binary file data>
```

**分片合并：**
```json
POST /api/files/upload/merge
{
    "file_id": 123,
    "total_chunks": 8
}
```

### 5.3 分享模块

| 方法 | 路径 | 功能 | 认证 |
|------|------|------|------|
| POST | `/api/shares` | 创建分享链接 | 是 |
| GET | `/api/shares/:code` | 获取分享信息 | 否 |
| POST | `/api/shares/:code/save` | 转存文件 | 是 |
| DELETE | `/api/shares/:id` | 取消分享 | 是 |
| GET | `/api/shares/ranking` | 分享排行榜 | 否 |
| GET | `/api/shares/my` | 我的分享 | 是 |

### 5.4 AI Agent 模块

| 方法 | 路径 | 功能 | 认证 |
|------|------|------|------|
| POST | `/api/agent/chat` | Agent 对话 | 是 |
| GET | `/api/agent/search` | 语义搜索 | 是 |
| GET | `/api/agent/history` | 获取对话历史 | 是 |

**Agent 对话请求：**
```json
POST /api/agent/chat
{
    "message": "帮我找一下上个月关于项目计划的文档",
    "session_id": "chat_xxxxx",
    "user_id": 1
}
```

**Agent 对话响应（流式）：**
```json
// SSE 流式返回
data: {"type": "thought", "content": "用户需要找上个月的项目计划文档，我来搜索文件..."}
data: {"type": "action", "tool": "search_files_by_semantic", "args": {"query": "项目计划 2025-05"}}
data: {"type": "observation", "content": "找到 2 个相关文件..."}
data: {"type": "final", "content": "找到了！以下是与\"项目计划\"相关的文件：\n1. /docs/2025-05-project-plan.md\n2. /docs/meeting-notes-2025-05.md"}
```

**语义搜索请求：**
```json
GET /api/agent/search?q=项目计划&limit=5
```

**语义搜索响应：**
```json
{
    "code": 0,
    "data": {
        "results": [
            {
                "file_id": 1,
                "filename": "2025-05-project-plan.md",
                "score": 0.92,
                "snippet": "本项目计划旨在..."
            }
        ],
        "total": 2
    }
}
```

---

## 6. 项目目录结构

```
FileAgent-Server/
├── CMakeLists.txt                  # 顶层 CMake 配置
├── vcpkg.json                      # vcpkg 依赖管理（drogon + nlohmann-json + ...）
├── Dockerfile                      # 多阶段构建 Dockerfile
├── docker-compose.yml              # docker-compose 编排
├── sql/
│   └── init.sql                    # 数据库建表脚本
│
├── src/
│   ├── main.cpp                    # 入口：Server 初始化 + 启动
│   │
│   ├── filter/
│   │   ├── AuthFilter.h/.cpp       # 认证过滤器（Session/Token 校验）
│   │   ├── LogFilter.h/.cpp        # 日志过滤器
│   │   └── RateLimitFilter.h/.cpp  # 限流过滤器
│   │
│   ├── controller/
│   │   ├── UserController.h/.cpp   # 用户相关 API
│   │   ├── FileController.h/.cpp   # 文件相关 API
│   │   ├── ShareController.h/.cpp  # 分享相关 API
│   │   └── AgentController.h/.cpp  # Agent 对话 API（SSE 流式）
│   │
│   ├── service/
│   │   ├── UserService.h/.cpp      # 用户业务逻辑
│   │   ├── FileService.h/.cpp      # 文件业务逻辑（秒传、分片）
│   │   ├── ShareService.h/.cpp     # 分享业务逻辑
│   │   └── AgentService.h/.cpp     # Agent 调度 + 会话管理
│   │
│   ├── dao/
│   │   ├── ConnectionPool.h/.cpp   # ★ MySQL 连接池
│   │   ├── TransactionGuard.h/.cpp # ★ 事务管理（RAII）
│   │   ├── UserDAO.h/.cpp          # 用户数据访问
│   │   ├── FileDAO.h/.cpp          # 文件数据访问
│   │   ├── ShareDAO.h/.cpp         # 分享数据访问
│   │   └── ChunkDAO.h/.cpp         # 分片数据访问
│   │
│   ├── agent/
│   │   ├── AgentEngine.h/.cpp      # ★ ReAct Agent Loop
│   │   ├── ToolRegistry.h/.cpp     # 工具注册表
│   │   ├── tools/
│   │   │   ├── SearchTool.h/.cpp   # 语义搜索工具
│   │   │   ├── FileInfoTool.h/.cpp # 文件信息工具
│   │   │   ├── FileListTool.h/.cpp # 文件列表工具
│   │   │   ├── SummarizeTool.h/.cpp# 文件总结工具
│   │   │   ├── OrganizeTool.h/.cpp # 文件整理工具
│   │   │   └── DeleteTool.h/.cpp   # 文件删除工具
│   │   └── RAGPipeline.h/.cpp      # ★ RAG 检索管道
│   │
│   ├── llm/
│   │   └── LLMClient.h/.cpp        # llama.cpp 推理 + embedding 封装
│   │
│   ├── vector/
│   │   ├── VectorStore.h/.cpp      # ★ 向量索引 + 余弦相似度检索
│   │   └── TextChunker.h/.cpp      # 文档分块器
│   │
│   ├── cache/
│   │   ├── LRUCache.h/.cpp         # ★ 自实现 LRU Cache
│   │   └── RedisCache.h/.cpp       # Redis 封装（hiredis）
│   │
│   └── common/
│       ├── Config.h/.cpp           # 配置文件解析
│       ├── Logger.h/.cpp           # 日志模块
│       ├── Crypto.h/.cpp           # SHA-256 / 哈希工具
│       └── Types.h                 # 公共类型定义
│
├── client/
│   ├── CMakeLists.txt              # CLI 客户端 CMake
│   ├── main.cpp                    # 客户端入口
│   ├── HttpClient.h/.cpp           # HTTP 客户端封装
│   └── AgentCli.h/.cpp             # 交互式 Agent CLI
│
└── tests/
    ├── CMakeLists.txt
    ├── test_connection_pool.cpp
    ├── test_dao.cpp
    ├── test_agent.cpp
    └── test_vector_store.cpp
```

---

## 7. 环境依赖

### 7.1 开发环境

| 工具 | 版本 | 说明 |
|------|------|------|
| GCC | 11+ | C++20 支持 |
| CMake | 3.20+ | 构建系统 |
| vcpkg | 最新 | 包管理 |
| MySQL | 8.0+ | 数据库 |
| Redis | 6.0+ | 缓存（可选，可用 LRU Cache 替代） |

### 7.2 第三方库

| 库 | 用途 | 获取方式 |
|----|------|---------|
| **Drogon** | HTTP 框架（epoll + 路由 + 中间件） | vcpkg |
| libmysqlclient | MySQL C API | 系统安装 / vcpkg |
| nlohmann/json | JSON 解析 | vcpkg |
| hiredis | Redis 客户端 | vcpkg（可选）|
| llama.cpp | LLM 推理 + embedding | Git Submodule 或 FetchContent |
| Google Test | 单元测试 | vcpkg |
| OpenSSL | SHA-256 哈希工具 | vcpkg |

### 7.3 vcpkg.json

```json
{
    "name": "fileagent-server",
    "version": "1.0.0",
    "dependencies": [
        "drogon",
        "nlohmann-json",
        "libmysqlclient",
        "hiredis",
        "gtest",
        "openssl"
    ]
}
```

---

## 8. 部署方案

### 8.1 Docker 多阶段构建

```dockerfile
# === 构建阶段 ===
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential cmake git pkg-config \
    libmysqlclient-dev libssl-dev libz-dev \
    && rm -rf /var/lib/apt/lists/*

# 安装 vcpkg + 依赖（drogon / nlohmann-json / ...）
RUN git clone https://github.com/Microsoft/vcpkg /opt/vcpkg && \
    /opt/vcpkg/bootstrap-vcpkg.sh

WORKDIR /build
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
RUN cmake --build build -j$(nproc)

# === 运行阶段 ===
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libmysqlclient-dev libssl-dev ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /build/build/fileagent-server /app/fileagent-server
COPY config.yaml /app/config.yaml

WORKDIR /app
EXPOSE 10086

CMD ["./fileagent-server"]
```

### 8.2 docker-compose.yml

```yaml
version: '3.8'

services:
  mysql:
    image: mysql:8.0
    environment:
      MYSQL_ROOT_PASSWORD: root123
      MYSQL_DATABASE: fileagent
      MYSQL_USER: fileagent
      MYSQL_PASSWORD: fileagent123
    ports:
      - "3306:3306"
    volumes:
      - mysql_data:/var/lib/mysql
      - ./sql/init.sql:/docker-entrypoint-initdb.d/init.sql
    networks:
      - fileagent-net

  redis:
    image: redis:7-alpine
    ports:
      - "6379:6379"
    volumes:
      - redis_data:/data
    networks:
      - fileagent-net

  fileagent-server:
    build: .
    ports:
    - "10086:10086"
    depends_on:
      - mysql
      - redis
    environment:
      DB_HOST: mysql
      DB_PORT: 3306
      DB_USER: fileagent
      DB_PASSWORD: fileagent123
      DB_NAME: fileagent
      REDIS_HOST: redis
      REDIS_PORT: 6379
      LLM_MODEL_PATH: /models/llama-2-7b.Q4_K_M.gguf
    volumes:
      - file_storage:/data/files
      - ./models:/models:ro
    networks:
      - fileagent-net

volumes:
  mysql_data:
  redis_data:
  file_storage:

networks:
  fileagent-net:
    driver: bridge
```

---

## 9. 开发路线图

### 第一阶段：基础设施搭建（第 1-2 周）

| 任务 | 产出 | 验证方式 |
|------|------|---------|
| CMake 工程搭建 + vcpkg 配置（含 Drogon） | 可编译的工程骨架 | `cmake --build` 成功 |
| 日志模块 + 配置解析 | Logger + Config | 日志输出正常 |
| **Drogon 框架集成**（路由 / 中间件 / 启动） | 可启动的 HTTP Server | `curl localhost:10086` 有响应 |
| 过滤器实现（认证 / 日志 / 限流） | 中间件链生效 | curl 携带/不携带 token 验证 |
| Config + Types + 公共基础设施 | 项目基础模块就绪 | 各模块可编译 |
| DAO + 连接池代码骨架集成 | 项目结构完整 | `cmake --build` 无链接错误 |

### 第二阶段：数据库 + 连接池（第 3 周）

| 任务 | 产出 | 验证方式 |
|------|------|---------|
| SQL 建表脚本 | database schema | MySQL 执行建表 |
| **MySQL 连接池** | 可用的连接池 |                      单元测试（并发获取/归还）|
| DAO 层基类 + 工具函数 | SQL 执行封装 | 单元测试通过 |
| UserDAO | 用户 CRUD | 集成测试 |
| FileDAO | 文件 CRUD + SHA-256 查询 | 集成测试 |
| ShareDAO + ChunkDAO | 分享 + 分片操作 | 集成测试 |

### 第三阶段：业务逻辑（第 4-5 周）

| 任务 | 产出 | 验证方式 |
|------|------|---------|
| 用户注册/登录/登出 API | 能注册登录 | curl 测试 |
| Session 管理 + 认证中间件 | 需要 token 才能访问 | curl 测试 |
| 文件上传（含秒传校验） | 能秒传 | 相同文件秒传验证 |
| 大文件分片上传 + 合并 | 能分片上传 | curl 分片测试 |
| 文件下载 + 删除 | 能下载删除 | curl 测试 |
| 分享创建 + 转存 + 取消 | 能分享文件 | curl 测试 |
| Redis / LRU Cache 集成 | 缓存生效 | 性能对比测试 |

### 第四阶段：AI Agent 引擎（第 6-7 周）

| 任务 | 产出 | 验证方式 |
|------|------|---------|
| llama.cpp 集成（推理） | 能本地跑 LLM | 命令行推理测试 |
| llama.cpp Embedding API | 能生成向量 | 向量维度正确 |
| LLMClient 封装 | 统一推理接口 | 单元测试通过 |
| **ReAct Agent Loop** | Agent 能多步推理 | 对话测试 |
| **ToolRegistry + 工具实现** | 5+ 工具可用 | 各工具独立测试 |
| **RAG 管道**（分块+向量化+检索） | 能语义搜索文件 | 搜索效果测试 |
| AgentController + API | 能通过 HTTP 对话 | curl 对话测试 |

### 第五阶段：客户端 + 完善（第 8 周）

| 任务 | 产出 | 验证方式 |
|------|------|---------|
| CLI 客户端（HTTP 调用） | 命令行能上传/搜索 | 端到端测试 |
| Agent CLI（交互式对话） | 命令行 Agent 对话 | 对话流测试 |
| Docker 多阶段构建 | 镜像构建成功 | `docker build` 通过 |
| docker-compose 编排 | 一键启动 | `docker-compose up` 验证 |
| 单元测试完善 | 覆盖率 > 70% | `ctest` 执行 |
| 性能压测 | 基础 QPS 数据 | wrk / ab 压测 |

---

## 10. 扩展方向（可选，不纳入核心需求）

以下是项目完成后可选的扩展方向，作为简历中的"后续规划"：

1. **WebSocket 支持**：Agent 对话改用 WebSocket，实现真正的流式交互
2. **gRPC 取代 HTTP**：内部服务通信改用 gRPC，支持多实例部署
3. **引入 HNSW 索引**：替代暴力搜索，支持百万级向量检索
4. **分布式文件存储**：参考 FastDFS 思路，支持多节点存储
5. **MCP 协议支持**：Agent 接入 MCP，扩展更多外部工具
6. **Web 管理界面**：提供浏览器端管理界面

---

## 11. 评估标准

### 11.1 功能完整性

| 检查项 | 是否完成 |
|-------|---------|
| HTTP Server（Drogon 框架：epoll + 中间件） | □ || MySQL 连接池（单例 + RAII + 心跳） | □ |
| DAO 层（4+ 数据表） | □ |
| 用户注册/登录/登出 | □ |
| 文件上传/下载/删除 | □ |
| SHA-256 秒传 | □ |
| 分片上传 + 合并 | □ |
| 文件分享 + 转存 | □ |
| 分享排行榜（Redis / LRU Cache） | □ |
| ReAct Agent Loop | □ |
| 5+ Agent 工具 | □ |
| RAG 管道（分块 + 向量化 + 检索） | □ |
| llama.cpp 集成 | □ |
| Docker 部署 | □ |
| CLI 客户端 | □ |

### 11.2 代码质量

- 代码规范：遵循 C++ Core Guidelines
- 异常安全：RAII 管理资源，异常中立
- 线程安全：连接池、缓存、向量存储线程安全
- 单元测试：核心模块有单元测试覆盖
- 内存管理：无内存泄漏（valgrind 验证）

---

## 附录 A：配置文件示例（config.yaml）

```yaml
server:
  host: "0.0.0.0"
    port: 10086
  threads: 4
  max_connections: 1024

database:
  host: "127.0.0.1"
  port: 3306
  user: "fileagent"
  password: "fileagent123"
  database: "fileagent"
  pool:
    min_size: 4
    max_size: 20
    timeout: 5
    idle_timeout: 300
    heartbeat_interval: 60

cache:
  type: "redis"       # redis / lru
  redis:
    host: "127.0.0.1"
    port: 6379
  lru:
    capacity: 10000

llm:
  model_path: "/models/llama-2-7b.Q4_K_M.gguf"
  embedding_model: "/models/bge-m3.Q4_K_M.gguf"
  n_gpu_layers: 0
  context_size: 4096
  temperature: 0.7
  max_tokens: 2048

agent:
  max_steps: 10
  timeout: 60
  system_prompt: >
    你是一个文件管理助手。你可以帮助用户搜索文件、查看文件信息、
    整理文件、总结文件内容。请根据用户的需求选择合适的工具。
    如果需要用户确认的操作（如删除文件），请先询问用户确认。
```

---

> **文档版本**：v1.0  
> **最后更新**：2026-06-29  
> **状态**：需求确认阶段
