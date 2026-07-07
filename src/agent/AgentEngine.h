#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace agents
{
class LLMInterface;
class Tool;
} // namespace agents

namespace fileagent
{

class VectorStore;

/**
 * @brief AI Agent 引擎 — ReAct Loop + 工具调用
 *
 * 基于 agents-cpp 的 LLMInterface + Tool 系统。
 * 注册文件管理工具，支持自然语言对话式文件操作。
 */
class AgentEngine
{
public:
    /**
     * @brief 初始化 Agent 引擎
     * @param llm LLM 接口
     * @param vector_store 向量存储引用
     */
    void init(std::shared_ptr<agents::LLMInterface> llm,
              VectorStore &vector_store);

    /**
     * @brief 发送消息并获取回复
     * @param user_message 用户消息
     * @param user_id 当前用户 ID
     * @return Agent 回复文本
     */
    std::string chat(const std::string &user_message, std::int64_t user_id);

    /// 是否已初始化
    bool initialized() const { return initialized_; }

private:
    bool initialized_{false};
    std::shared_ptr<agents::LLMInterface> llm_;
    VectorStore *vector_store_{nullptr};
    std::vector<std::shared_ptr<agents::Tool>> tools_;
};

} // namespace fileagent
