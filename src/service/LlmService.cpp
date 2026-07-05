#include "LlmService.h"

#include <agents-cpp/llm_interface.h>

#include "../common/Logger.h"

namespace fileagent
{

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
                           const std::string &model)
    {
        provider_ = provider;
        api_key_ = api_key;
        api_base_ = api_base;
        model_ = model;

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

} // namespace fileagent
