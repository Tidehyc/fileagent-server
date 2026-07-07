#include "AgentEngine.h"

#include <fstream>
#include <vector>

#include <agents-cpp/llm_interface.h>
#include <agents-cpp/tool.h>
#include <agents-cpp/types.h>

#include "../common/Logger.h"
#include "../dao/FileDao.h"
#include "../dao/Models.h"
#include "../vector/VectorStore.h"

namespace fileagent
{

void AgentEngine::init(std::shared_ptr<agents::LLMInterface> llm,
                        VectorStore &vector_store)
{
    if (!llm)
    {
        logError("AgentEngine: LLM is null");
        return;
    }

    llm_ = llm;
    vector_store_ = &vector_store;

    // ─── 注册工具 ──────────────────────────────────

    using agents::Parameter;

    // 1. 语义搜索工具（引导用户使用 API）
    auto search_tool = agents::createTool(
        "semantic_search",
        "通过自然语言搜索文件，找到内容相关的文件",
        {
            {"query", "搜索关键词或自然语言描述", "string", true},
            {"limit", "返回结果数量", "integer", false, 5}
        },
        [this](const agents::JsonObject &params) -> agents::ToolResult
        {
            if (!vector_store_)
                return {false, "vector store not available", {}};

            std::string query;
            if (params.contains("query") && params["query"].is_string())
                query = params["query"].get<std::string>();

            if (query.empty())
                return {false, "query is required", {}};

            std::string result = "请使用 POST /api/search 接口进行语义搜索，关键词: " + query;
            return {true, result, {}};
        }
    );
    tools_.push_back(search_tool);

    // 2. 文件信息工具
    auto info_tool = agents::createTool(
        "get_file_info",
        "获取文件的详细信息，包括文件名、大小、摘要和标签",
        {
            {"file_id", "文件ID", "integer", true}
        },
        [](const agents::JsonObject &params) -> agents::ToolResult
        {
            if (!params.contains("file_id") || !params["file_id"].is_number())
                return {false, "file_id is required", {}};

            std::int64_t file_id = params["file_id"].get<std::int64_t>();
            FileDao file_dao;
            auto file = file_dao.findById(file_id);
            if (!file.has_value())
                return {false, "file not found", {}};

            agents::JsonObject data;
            data["filename"] = file->filename;
            data["file_size"] = std::to_string(file->file_size) + " bytes";
            data["hash"] = file->file_hash;
            data["summary"] = file->summary.empty() ? "暂无摘要" : file->summary;
            data["tags"] = file->tags.empty() ? "暂无标签" : file->tags;
            data["created_at"] = file->created_at;

            std::string content = "文件: " + file->filename +
                                  "\n大小: " + data["file_size"].get<std::string>() +
                                  "\n摘要: " + data["summary"].get<std::string>() +
                                  "\n标签: " + data["tags"].get<std::string>() +
                                  "\n创建时间: " + file->created_at;

            return {true, content, data};
        }
    );
    tools_.push_back(info_tool);

    // 3. 列出文件工具
    auto list_tool = agents::createTool(
        "list_files",
        "列出用户的文件列表",
        {
            {"user_id", "用户ID", "integer", true},
            {"limit", "返回数量", "integer", false, 20},
            {"offset", "偏移量", "integer", false, 0}
        },
        [](const agents::JsonObject &params) -> agents::ToolResult
        {
            if (!params.contains("user_id") || !params["user_id"].is_number())
                return {false, "user_id is required", {}};

            std::int64_t user_id = params["user_id"].get<std::int64_t>();
            int limit = params.contains("limit") ? params["limit"].get<int>() : 20;
            int offset = params.contains("offset") ? params["offset"].get<int>() : 0;

            FileDao file_dao;
            auto files = file_dao.listByUserId(user_id, limit, offset);

            std::string content = "共找到 " + std::to_string(files.size()) + " 个文件:\n";
            for (const auto &f : files)
            {
                content += "- [" + std::to_string(f.id) + "] " + f.filename +
                           " (" + std::to_string(f.file_size) + " bytes)\n";
            }

            agents::JsonObject data;
            data["total"] = static_cast<int>(files.size());
            return {true, content, data};
        }
    );
    tools_.push_back(list_tool);

    // 4. 文件内容总结工具
    auto summarize_tool = agents::createTool(
        "summarize_file",
        "读取文件内容并用 AI 进行总结",
        {
            {"file_id", "文件ID", "integer", true}
        },
        [this](const agents::JsonObject &params) -> agents::ToolResult
        {
            if (!params.contains("file_id") || !params["file_id"].is_number())
                return {false, "file_id is required", {}};

            std::int64_t file_id = params["file_id"].get<std::int64_t>();
            FileDao file_dao;
            auto file = file_dao.findById(file_id);
            if (!file.has_value())
                return {false, "file not found", {}};

            std::string content;
            std::ifstream in(file->storage_path, std::ios::binary | std::ios::ate);
            if (!in)
                return {false, "cannot read file content", {}};

            auto size = in.tellg();
            in.seekg(0);
            if (size > 102400)
                size = 102400;
            content.resize(static_cast<size_t>(size));
            in.read(content.data(), size);

            try
            {
                auto response = llm_->chat(
                    "请用几句话总结以下文件内容：\n\n```\n" +
                    content.substr(0, 10000) + "\n```\n\n总结：");
                return {true, response.content, {{"summary", response.content}}};
            }
            catch (const std::exception &e)
            {
                logError("AgentEngine::summarize LLM failed: " + std::string(e.what()));
                return {false, std::string("LLM summarize failed: ") + e.what(), {}};
            }
        }
    );
    tools_.push_back(summarize_tool);

    initialized_ = true;
    logInfo("AgentEngine initialized with " + std::to_string(tools_.size()) + " tools");
}

std::string AgentEngine::chat(const std::string &user_message, std::int64_t user_id)
{
    if (!initialized_ || !llm_)
        return "Agent not initialized";

    try
    {
        // 构建系统提示词
        std::string system_prompt =
            "你是一个文件管理助手。你可以帮助用户搜索文件、查看文件信息、列出文件、总结文件内容。\n"
            "用户可能用中文提问，请用中文回答。\n"
            "当用户的问题涉及具体文件操作时，选择相应的工具来执行。\n"
            "当前用户ID: " + std::to_string(user_id);

        // 构建消息历史
        std::vector<agents::Message> messages;
        agents::Message sys_msg;
        sys_msg.role = agents::Message::Role::SYSTEM;
        sys_msg.content = system_prompt;
        messages.push_back(std::move(sys_msg));

        agents::Message user_msg;
        user_msg.role = agents::Message::Role::USER;
        user_msg.content = user_message;
        messages.push_back(std::move(user_msg));

        // 使用 LLMInterface 的 chatWithTools（同步调用，自动处理工具调用）
        auto response = llm_->chatWithTools(messages, tools_);
        return response.content;
    }
    catch (const std::exception &e)
    {
        logError("AgentEngine::chat failed: " + std::string(e.what()));
        return std::string("抱歉，处理时出错: ") + e.what();
    }
}

} // namespace fileagent
