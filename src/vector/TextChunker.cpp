#include "TextChunker.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>

namespace fileagent
{

std::vector<TextChunker::Chunk> TextChunker::chunkBySize(
    const std::string &text,
    const std::string &source_file,
    std::int64_t file_id,
    size_t chunk_size,
    size_t overlap)
{
    std::vector<Chunk> chunks;

    if (text.empty())
        return chunks;

    // 确保参数合理
    if (chunk_size == 0)
        chunk_size = 1000;
    if (overlap >= chunk_size)
        overlap = chunk_size / 4;

    // 对过短的文本，直接作为一块
    if (text.size() <= chunk_size)
    {
        Chunk c;
        c.content = text;
        c.index = 0;
        c.file_id = file_id;
        c.filename = source_file;
        chunks.push_back(std::move(c));
        return chunks;
    }

    size_t start = 0;
    int idx = 0;

    while (start < text.size())
    {
        size_t end = std::min(start + chunk_size, text.size());

        // 如果不是最后一块，尝试在 chunk_size 范围内找最近的换行符作为结束点
        if (end < text.size())
        {
            auto newline_pos = text.rfind('\n', end);
            if (newline_pos != std::string::npos && newline_pos > start + chunk_size / 2)
            {
                end = newline_pos + 1; // 包含换行符
            }
        }

        Chunk c;
        c.content = text.substr(start, end - start);
        c.index = idx++;
        c.file_id = file_id;
        c.filename = source_file;
        chunks.push_back(std::move(c));

        // 滑动到下一块（减去重叠长度）
        if (end >= text.size())
            break;

        start = end - overlap;
        // 如果重叠导致回退太多，至少前进 chunk_size/4
        if (start + chunk_size / 4 >= end)
            start = end - chunk_size / 4;
    }

    return chunks;
}

std::vector<TextChunker::Chunk> TextChunker::chunkByParagraph(
    const std::string &text,
    const std::string &source_file,
    std::int64_t file_id,
    size_t max_chunk_size)
{
    std::vector<Chunk> chunks;

    if (text.empty())
        return chunks;

    if (max_chunk_size == 0)
        max_chunk_size = 2000;

    // 如果文本很短，直接作为一块
    if (text.size() <= max_chunk_size)
    {
        Chunk c;
        c.content = text;
        c.index = 0;
        c.file_id = file_id;
        c.filename = source_file;
        chunks.push_back(std::move(c));
        return chunks;
    }

    // 按空行分割
    std::vector<std::string> paragraphs;
    std::istringstream stream(text);
    std::string line;
    std::string current_para;

    while (std::getline(stream, line))
    {
        if (line.empty() && !current_para.empty())
        {
            // 空行 = 段落结束
            paragraphs.push_back(current_para);
            current_para.clear();
        }
        else
        {
            current_para += line + "\n";
        }
    }
    if (!current_para.empty())
        paragraphs.push_back(current_para);

    // 将段落合并为块，每块不超过 max_chunk_size
    int idx = 0;
    std::string current_chunk;

    for (const auto &para : paragraphs)
    {
        if (current_chunk.size() + para.size() > max_chunk_size && !current_chunk.empty())
        {
            Chunk c;
            c.content = current_chunk;
            c.index = idx++;
            c.file_id = file_id;
            c.filename = source_file;
            chunks.push_back(std::move(c));
            current_chunk.clear();
        }

        if (para.size() > max_chunk_size)
        {
            // 超长段落，用 chunkBySize 递归处理
            auto sub_chunks = chunkBySize(para, source_file, file_id, max_chunk_size, max_chunk_size / 5);
            for (auto &sc : sub_chunks)
            {
                sc.index = idx++;
                chunks.push_back(std::move(sc));
            }
        }
        else
        {
            current_chunk += para;
        }
    }

    if (!current_chunk.empty())
    {
        Chunk c;
        c.content = current_chunk;
        c.index = idx++;
        c.file_id = file_id;
        c.filename = source_file;
        chunks.push_back(std::move(c));
    }

    return chunks;
}

} // namespace fileagent
