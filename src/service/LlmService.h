#pragma once

#include <memory>
#include <string>

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
         * @param provider   "ollama" 或 "local"
         * @param api_base   Ollama 服务地址
         * @param model      模型名称
         */
        void init(const std::string &provider,
                  const std::string &api_base,
                  const std::string &model);

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
         * @brief 是否已初始化
         */
        bool initialized() const { return initialized_; }

    private:
        bool initialized_{false};
        std::string provider_;
        std::string api_base_;
        std::string model_;

        // 前向声明（OllamaLLM 完整定义在 agents-cpp 中）
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace fileagent
