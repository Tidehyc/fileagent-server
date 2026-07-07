#include "LlmService.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

#include <agents-cpp/llm_interface.h>

#include "../common/Logger.h"

namespace fileagent
{

// ─── 内联 HTTP 辅助函数 ─────────────────────────────
namespace
{

// 简单的 HTTP POST 请求（JSON in → JSON out）
// 用于调用 Ollama API，不需要额外的 HTTP 客户端库
bool httpPostJson(const std::string &host, int port,
                  const std::string &path,
                  const std::string &json_body,
                  std::string &response_body)
{
    // 解析主机名
    struct addrinfo hints, *res;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port);
    int status = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
    if (status != 0)
        return false;

    // 创建 socket
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0)
    {
        freeaddrinfo(res);
        return false;
    }

    // 连接
    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0)
    {
        close(sock);
        freeaddrinfo(res);
        return false;
    }

    freeaddrinfo(res);

    // 超时设置
    struct timeval tv;
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    // 构造 HTTP 请求
    std::ostringstream req;
    req << "POST " << path << " HTTP/1.1\r\n"
        << "Host: " << host << ":" << port << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << json_body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << json_body;

    std::string request = req.str();

    // 发送
    size_t total_sent = 0;
    while (total_sent < request.size())
    {
        ssize_t n = send(sock, request.data() + total_sent,
                         request.size() - total_sent, 0);
        if (n <= 0)
        {
            close(sock);
            return false;
        }
        total_sent += static_cast<size_t>(n);
    }

    // 接收响应
    std::string raw;
    char buf[8192];
    ssize_t bytes;
    while ((bytes = recv(sock, buf, sizeof(buf) - 1, 0)) > 0)
    {
        buf[bytes] = '\0';
        raw += buf;
    }

    close(sock);

    // 解析 HTTP 响应，提取 body
    auto body_pos = raw.find("\r\n\r\n");
    if (body_pos == std::string::npos)
        return false;

    // 检查状态码
    auto space1 = raw.find(' ');
    auto space2 = raw.find(' ', space1 + 1);
    int http_code = 0;
    if (space1 != std::string::npos && space2 != std::string::npos)
    {
        http_code = std::stoi(raw.substr(space1 + 1, space2 - space1 - 1));
    }

    response_body = raw.substr(body_pos + 4);
    return http_code == 200;
}

} // anonymous namespace

// ─── PIMPL 实现隐藏 agents-cpp 类型 ───────────
class LlmService::Impl
{
public:
    void init(const std::string &provider,
              const std::string &api_key,
              const std::string &api_base,
              const std::string &model)
    {
        // 使用 agents-cpp 工厂函数创建对应 provider 的 LLM 实例
        llm_ = agents::createLLM(provider, api_key, model);

        if (!llm_)
        {
            logError("LlmService: failed to create LLM for provider=" + provider);
            return;
        }

        // 设置自定义 API 地址（Ollama 或自定义代理）
        if (!api_base.empty())
        {
            llm_->setApiBase(api_base);
        }

        // 设置参数
        agents::LLMOptions opts;
        opts.temperature = 0.7;
        opts.max_tokens = 2048;
        llm_->setOptions(opts);

        logInfo("LlmService: initialized provider=" + provider +
                " model=" + model +
                (api_base.empty() ? "" : " api_base=" + api_base));
    }

    ServiceResult<std::string> chat(const std::string &prompt)
    {
        if (!llm_)
        {
            return ServiceResult<std::string>::fail("LLM not initialized");
        }

        try
        {
            auto response = llm_->chat(prompt);
            return ServiceResult<std::string>::ok(response.content, "success");
        }
        catch (const std::exception &e)
        {
            logError("LlmService::chat failed: " + std::string(e.what()));
            return ServiceResult<std::string>::fail(std::string("LLM chat failed: ") + e.what());
        }
    }

    std::shared_ptr<agents::LLMInterface> getLLM() const
    {
        return llm_;
    }

    ServiceResult<std::string> analyze(const std::string &file_content,
                                        const std::string &instruction)
    {
        if (!llm_)
        {
            return ServiceResult<std::string>::fail("LLM not initialized");
        }

        std::string prompt = "请分析以下文件内容。\n\n";
        prompt += "分析要求：" + instruction + "\n\n";
        prompt += "文件内容：\n```\n" + file_content + "\n```\n\n";
        prompt += "请基于以上内容给出分析结果。";

        try
        {
            auto response = llm_->chat(prompt);
            return ServiceResult<std::string>::ok(response.content, "success");
        }
        catch (const std::exception &e)
        {
            logError("LlmService::analyze failed: " + std::string(e.what()));
            return ServiceResult<std::string>::fail(std::string("LLM analysis failed: ") + e.what());
        }
    }

private:
    std::shared_ptr<agents::LLMInterface> llm_;
};

// ─── LlmService 公开方法 ─────────────────────

LlmService::LlmService() = default;
LlmService::~LlmService() = default;

void LlmService::init(const std::string &provider,
                       const std::string &api_key,
                       const std::string &api_base,
                       const std::string &model,
                       const std::string &embedding_model)
{
    provider_ = provider;
    api_key_ = api_key;
    api_base_ = api_base;
    model_ = model;
    embedding_model_ = embedding_model.empty() ? model : embedding_model;

    impl_ = std::make_unique<Impl>();
    impl_->init(provider_, api_key_, api_base_, model_);
    initialized_ = true;
}

ServiceResult<std::string> LlmService::chat(const std::string &prompt)
{
    if (!initialized_ || !impl_)
    {
        return ServiceResult<std::string>::fail("LLM not initialized");
    }
    return impl_->chat(prompt);
}

ServiceResult<std::string> LlmService::analyze(const std::string &file_content,
                                                const std::string &instruction)
{
    if (!initialized_ || !impl_)
    {
        return ServiceResult<std::string>::fail("LLM not initialized");
    }
    return impl_->analyze(file_content, instruction);
}

std::shared_ptr<agents::LLMInterface> LlmService::getLLM() const
{
    if (!impl_)
        return nullptr;
    return impl_->getLLM();
}

ServiceResult<std::vector<float>> LlmService::embed(const std::string &text)
{
    if (!initialized_)
    {
        return ServiceResult<std::vector<float>>::fail("LLM not initialized");
    }

    if (text.empty())
    {
        return ServiceResult<std::vector<float>>::fail("empty text");
    }

    // 解析 api_base URL: "http://host:port/path"
    std::string host = "localhost";
    int port = 11434;
    std::string path = "/api/embed";

    if (!api_base_.empty())
    {
        // 简单 URL 解析
        std::string url = api_base_;
        // 去掉 http:// 前缀
        if (url.find("http://") == 0)
            url = url.substr(7);
        else if (url.find("https://") == 0)
            url = url.substr(8);

        // 分离 host:port 和 path
        auto slash = url.find('/');
        if (slash != std::string::npos)
        {
            path = url.substr(slash);
            url = url.substr(0, slash);
        }

        // 分离 host 和 port
        auto colon = url.find(':');
        if (colon != std::string::npos)
        {
            host = url.substr(0, colon);
            port = std::stoi(url.substr(colon + 1));
        }
        else
        {
            host = url;
        }
    }

    // 构造 Ollama embedding API 请求
    nlohmann::json req_body;
    req_body["model"] = embedding_model_;
    req_body["input"] = text;

    std::string resp_body;
    bool ok = httpPostJson(host, port, path, req_body.dump(), resp_body);

    if (!ok)
    {
        logError("LlmService::embed: HTTP request failed");
        return ServiceResult<std::vector<float>>::fail("embedding request failed");
    }

    try
    {
        auto json_resp = nlohmann::json::parse(resp_body);

        // Ollama /api/embed 返回: {"embeddings": [[...]], ...}
        if (!json_resp.contains("embeddings") || json_resp["embeddings"].empty())
        {
            logError("LlmService::embed: no embeddings in response");
            return ServiceResult<std::vector<float>>::fail("no embeddings returned");
        }

        std::vector<float> embedding;
        auto &first = json_resp["embeddings"][0];
        for (const auto &val : first)
        {
            embedding.push_back(val.get<float>());
        }

        logInfo("LlmService::embed: got " + std::to_string(embedding.size()) +
                "-dim embedding for " + std::to_string(text.size()) + " chars");
        return ServiceResult<std::vector<float>>::ok(std::move(embedding), "success");
    }
    catch (const std::exception &e)
    {
        logError("LlmService::embed: parse failed: " + std::string(e.what()));
        return ServiceResult<std::vector<float>>::fail("failed to parse embedding response");
    }
}

} // namespace fileagent
