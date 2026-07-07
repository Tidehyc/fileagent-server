#pragma once

#include <string>

namespace fileagent
{

/**
 * @brief 文件文本提取器
 *
 * 根据文件扩展名选择提取方式：
 * - 文本类 (.txt/.md/.cpp/.h/.py/.js/...) → UTF-8 直接读取
 * - PDF (.pdf) → 调用 pdftotext 命令
 * - 其他 → 返回空（不支持提取）
 */
class TextExtractor
{
public:
    /// 提取结果
    struct ExtractResult
    {
        std::string text;       // 提取的纯文本（提取失败或跳过时为空）
        std::string mime_type;  // 检测到的 MIME 类型
        bool success{false};    // 是否成功提取
    };

    /**
     * @brief 从文件内容中提取纯文本
     * @param content  文件原始二进制内容
     * @param filename 原始文件名（用于判断扩展名）
     * @return 提取结果
     */
    static ExtractResult extract(const std::string &content, const std::string &filename);

    /**
     * @brief 判断是否为可提取文本的文件类型
     */
    static bool isTextFile(const std::string &filename);

    /**
     * @brief 判断是否为 PDF 文件
     */
    static bool isPdfFile(const std::string &filename);

private:
    /// 获取文件扩展名（小写）
    static std::string getExtension(const std::string &filename);

    /// 从 PDF 内容提取文本（调用 pdftotext）
    static ExtractResult extractPdf(const std::string &content, const std::string &filename);

    /// 从纯文本内容提取
    static ExtractResult extractText(const std::string &content, const std::string &filename);
};

} // namespace fileagent
