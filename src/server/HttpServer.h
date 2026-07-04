#pragma once

#include <drogon/drogon.h>

// Drogon 框架接管 HTTP Server 层，此目录只保留自定义 Filter。
//
// Drogon 底层架构：
//   - epoll ET + 非阻塞 IO（Linux）
//   - 主 Reactor（accept） + 多 Worker（IO 事件处理）
//   - 支持协程（C++20 Coroutines）
//
// 使用方式（在 main.cpp 中）：
//
//   using namespace drogon;
//   app().registerHandler("/api/user/login", ..., {Post});
//   app().addListener("0.0.0.0", 10086).setThreadNum(4).run();