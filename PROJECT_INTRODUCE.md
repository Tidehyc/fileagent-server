# FileAgent Server — AI 智能云存储文件系统

---

**项目时间**：2025-05 ~ 2025-08  
**角色**：独立开发者

---

## 项目描述

本项目是基于 **C++20** 在 Linux 环境下开发的高性能 AI 智能云存储文件系统。采用 **Nginx + FastCGI** 架构提供 HTTP 服务，**FastDFS** 分布式文件系统存储用户文件，**MySQL + Redis** 完成数据持久化与缓存。项目深度融合 AI 能力，通过 **Ollama Embedding API** + 自实现向量检索引擎实现文件级语义搜索，并基于 **agents-cpp** 构建了 **ReAct Agent 对话引擎**，用户可通过自然语言与系统交互完成文件管理操作。

## 核心技术与中间件

**C++20、Nginx、FastCGI（libfcgi）、FastDFS、MySQL 8.0、Redis、Ollama、agents-cpp、nlohmann/json、yaml-cpp、OpenSSL、Docker**

## 主要内容

### 1. Nginx + FastCGI 高性能 HTTP 架构

Nginx 反向代理将动态请求通过 FastCGI 协议转发至 C++ 后端，专一处理静态文件服务，实现**动静分离**。X-Accel-Redirect 实现认证后零拷贝文件下发，FastCGI 主循环完成 HTTP 请求解析与路由分发。

### 2. 分布式存储（FastDFS + 本地磁盘双后端）

集成 FastDFS 分布式文件系统，封装 `FastDfsClient` SDK。双后端自适应——新文件存储到 FastDFS，旧文件保留本地磁盘，通过 `storage_path` 前缀自动判断后端（`group` 开头 → FastDFS，`uploads/` 开头 → 本地磁盘），对业务层完全透明。

### 3. 数据库连接池与缓存

**自实现 MySQL 连接池**——单例模式、RAII `ConnectionGuard` 守卫自动获取/归还、生产者消费者模型、空闲连接心跳保活与自动扩缩容。Redis / LRU Cache 双模切换，Redis 不可用时无缝降级。

### 4. 用户认证与文件管理

用户 bcrypt 密码注册登录、Session Token 认证、文件 SHA-256 去重上传、大文件分片上传（分片 → 序号合并）、秒传预检 API（`POST /api/files/check`）、分享链接生成与转存、分享排行榜。

### 5. AI 语义搜索

Ollama Embedding API + 自实现向量索引引擎：文件上传 → `TextExtractor` 提取纯文本 → `TextChunker` 分块 → Embedding 向量化 → `VectorStore`（余弦相似度 Top-K 检索）。上传时自动调用 LLM 生成摘要和标签，持久化到 MySQL。

### 6. ReAct Agent 对话引擎

基于 agents-cpp `LLMInterface::chatWithTools()` 构建，注册 4 个工具（`semantic_search` / `get_file_info` / `list_files` / `summarize_file`）。LLM 自动判断用户意图并调用对应工具，多步推理后给出回答。支持自然语言文件操作。

### 7. 容器化部署与工程化

Dockerfile 多阶段构建 + docker-compose 编排 MySQL/Redis/应用三服务。配套 25 个单元测试（Google Test）、滑动窗口 Rate Limiter 防暴力破解、`PreparedStatement` RAII 封装防 SQL 注入。

## 项目难点

1. **FastDFS 双后端自适应**——本地磁盘与 FastDFS 路径前缀自动识别，业务层无需关心存储后端差异，迁移过程对用户完全透明
2. **文件秒传**——客户端上传前 SHA-256 预检，已存在文件直接引用，避免重复存储
3. **大文件分片上传**——文件分割为有序分片，全部上传完成后再按序号顺序合并，支持断点续传与进度查询
4. **分享转存与排行榜**——MySQL 维护分享关联关系，Redis ZSET 维护文件热度排名，转存自动去重
5. **AI 语义搜索链路**——文本提取 → 分块 → Embedding → 向量检索，全程自实现无需外部向量数据库
6. **Agent 自然语言交互**——LLM 意图识别 + 工具调用 + 多步推理，用户可通过自然语言完成文件查找和管理

## 个人收获

1. 深入理解了 **Nginx 高性能 Web 服务器**原理（反向代理、Worker 进程模型、epoll 事件驱动）以及 **FastCGI 协议**的通信机制
2. 通过 **FastDFS** 学习分布式文件系统的核心设计（Tracker 调度策略、Storage 同步机制、分组存储架构）
3. 实现了完整的 **MySQL 连接池**（生产者消费者模型、RAII 守卫、心跳保活、自动扩缩容）
4. 掌握了 **Embedding + 向量检索**的 RAG 技术链路，以及 **ReAct Agent** 的 Thought-Action-Observation 循环
5. 对 **AI 与业务结合**有了深入实践——为传统文件存储系统赋能语义搜索与智能对话能力
