#include "LlmService.h"

#include <agents-cpp/llms/ollama_llm.h>

#include "../common/Logger.h"

namespace fileagent
{

    // ─── PIMPL 实现隐藏 agents-cpp 类型 ───────────
    class LlmService::Impl
    {
    public:
        void init(const std::string &api_base, const std::string &model)
        {
            llm_ = std::make_unique<agents::llms::OllamaLLM>("", model);
            llm_->setApiBase(api_base);

            agents::LLMOptions opts;
            opts.temperature = 0.7;
            opts.max_tokens = 2048;
            llm_->setOptions(opts);
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
        std::unique_ptr<agents::llms::OllamaLLM> llm_;
    };

    // ─── LlmService 公开方法 ─────────────────────

    LlmService::LlmService() = default;
    LlmService::~LlmService() = default;

    void LlmService::init(const std::string &provider,
                           const std::string &api_base,
                           const std::string &model)
    {
        provider_ = provider;
        api_base_ = api_base;
        model_ = model;

        if (provider_ == "ollama")
        {
            impl_ = std::make_unique<Impl>();
            impl_->init(api_base_, model_);
            initialized_ = true;
            logInfo("LlmService: initialized with provider=" + provider_ +
                    " api_base=" + api_base_ + " model=" + model_);
        }
        else
        {
            logWarn("LlmService: provider '" + provider_ + "' not yet supported");
        }
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
