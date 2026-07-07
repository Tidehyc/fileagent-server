#pragma once

#include <memory>
#include <string>
#include <vector>

namespace agents { class LLMInterface; }

#include "ServiceTypes.h"

namespace fileagent
{

/**
 * @brief LLM 服务封装
 *
 * 封装 OllamaLLM 调用，提供对话和文件分析功能。
 * 运行时根据配置创建对应的 LLM 后端。
 */
class LlmService
{
public:
    LlmService();
    ~LlmService();

    /**
     * @brief 初始化 LLM 后端
     * @param provider   "ollama"/"openai"/"anthropic"/"google"/"local"
     * @param api_key    API Key（云服务需要）
     * @param api_base   自定义 API 地址（可选）
     * @param model      模型名称
     * @param embedding_model Embedding 模型名（空则用 model）
     */
    void init(const std::string &provider,
              const std::string &api_key,
              const std::string &api_base,
              const std::string &model,
              const std::string &embedding_model = "");

    /**
     * @brief 发送对话请求
     * @param prompt 用户提示
     * @return 回复文本
     */
    ServiceResult<std::string> chat(const std::string &prompt);

    /**
     * @brief 分析文件内容
     * @param file_content 文件文本内容
     * @param instruction  分析指令
     * @return 分析结果
     */
    ServiceResult<std::string> analyze(const std::string &file_content,
                                        const std::string &instruction);

    /**
     * @brief 生成文本的向量表示（Embedding）
     * @param text 输入文本
     * @return 浮点数向量，失败时返回错误
     */
    ServiceResult<std::vector<float>> embed(const std::string &text);

    /**
     * @brief 获取底层 LLMInterface（供 Agent 引擎使用）
     */
    std::shared_ptr<agents::LLMInterface> getLLM() const;

    /**
     * @brief 是否已初始化
     */
    bool initialized() const { return initialized_; }

private:
    bool initialized_{false};
    std::string provider_;
    std::string api_key_;
    std::string api_base_;
    std::string model_;
    std::string embedding_model_;

    // 前向声明（OllamaLLM 完整定义在 agents-cpp 中）
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fileagent
