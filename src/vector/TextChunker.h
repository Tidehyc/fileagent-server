#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fileagent
{

/**
 * @brief 文档分块器
 *
 * 支持两种分块策略：
 * 1. 滑动窗口分块 — 固定 token 数 + 重叠
 * 2. 按段落分块（适合 Markdown/文本）
 */
class TextChunker
{
public:
    struct Chunk
    {
        std::string content;    // 块文本
        int index{0};           // 块序号
        std::int64_t file_id{0}; // 所属文件 ID
        std::string filename;   // 原文件名
    };

    /**
     * @brief 滑动窗口分块
     * @param text 输入文本
     * @param source_file 源文件名
     * @param chunk_size 每块字符数
     * @param overlap 块间重叠字符数
     */
    static std::vector<Chunk> chunkBySize(
        const std::string &text,
        const std::string &source_file,
        std::int64_t file_id,
        size_t chunk_size = 1000,
        size_t overlap = 200);

    /**
     * @brief 按段落分块（以空行分隔）
     */
    static std::vector<Chunk> chunkByParagraph(
        const std::string &text,
        const std::string &source_file,
        std::int64_t file_id,
        size_t max_chunk_size = 2000);
};

} // namespace fileagent
