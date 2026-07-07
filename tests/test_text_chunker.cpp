#include <gtest/gtest.h>

#include "vector/TextChunker.h"

using namespace fileagent;

TEST(TextChunkerTest, EmptyText)
{
    auto chunks = TextChunker::chunkBySize("", "test.txt", 1);
    EXPECT_TRUE(chunks.empty());
}

TEST(TextChunkerTest, ShortText)
{
    auto chunks = TextChunker::chunkBySize("hello world", "test.txt", 1, 1000);
    EXPECT_EQ(chunks.size(), 1);
    EXPECT_EQ(chunks[0].content, "hello world");
    EXPECT_EQ(chunks[0].index, 0);
    EXPECT_EQ(chunks[0].file_id, 1);
    EXPECT_EQ(chunks[0].filename, "test.txt");
}

TEST(TextChunkerTest, MultipleChunks)
{
    // 生成超过 chunk_size 的文本
    std::string text;
    for (int i = 0; i < 100; i++)
    {
        text += "line " + std::to_string(i) + "\n";
    }

    auto chunks = TextChunker::chunkBySize(text, "test.txt", 1, 200, 20);
    EXPECT_GT(chunks.size(), 1);

    // 所有块应该属于同一个文件
    for (const auto &c : chunks)
    {
        EXPECT_EQ(c.file_id, 1);
        EXPECT_EQ(c.filename, "test.txt");
    }

    // 索引应该连续
    for (size_t i = 0; i < chunks.size(); i++)
    {
        EXPECT_EQ(chunks[i].index, static_cast<int>(i));
    }
}

TEST(TextChunkerTest, ChunkByParagraph)
{
    std::string text = "para one\n\npara two\n\npara three";

    // 用很小的 max_chunk_size 确保每个段落独立成块
    auto chunks = TextChunker::chunkByParagraph(text, "test.txt", 1, 10);
    EXPECT_EQ(chunks.size(), 3);
    EXPECT_EQ(chunks[0].content, "para one\n");
    EXPECT_EQ(chunks[1].content, "para two\n");
}

TEST(TextChunkerTest, ChunkByParagraphShortText)
{
    auto chunks = TextChunker::chunkByParagraph("just one paragraph", "test.txt", 1, 100);
    EXPECT_EQ(chunks.size(), 1);
    EXPECT_EQ(chunks[0].content, "just one paragraph");
}

TEST(TextChunkerTest, ChunkByParagraphLargeMaxSize)
{
    // max_chunk_size 很大时，所有段落合并为一块
    std::string text = "para one\n\npara two\n\npara three";
    auto chunks = TextChunker::chunkByParagraph(text, "test.txt", 1, 1000);
    EXPECT_EQ(chunks.size(), 1);
}

TEST(TextChunkerTest, OverlapLessThanChunkSize)
{
    auto chunks = TextChunker::chunkBySize("hello world and goodbye", "test.txt", 1, 10, 2);
    EXPECT_GT(chunks.size(), 1);
}

TEST(TextChunkerTest, ParametersValidation)
{
    // chunk_size=0 不崩溃（内部修复为默认值）
    auto chunks = TextChunker::chunkBySize("test text", "test.txt", 1, 0);
    EXPECT_EQ(chunks.size(), 1);

    // overlap >= chunk_size 不崩溃
    auto chunks2 = TextChunker::chunkBySize("a longer test text here", "test.txt", 1, 5, 10);
    EXPECT_GT(chunks2.size(), 0);
}

TEST(TextChunkerTest, FileIdAndFilename)
{
    auto chunks = TextChunker::chunkBySize("content", "myfile.txt", 42, 100);
    ASSERT_EQ(chunks.size(), 1);
    EXPECT_EQ(chunks[0].file_id, 42);
    EXPECT_EQ(chunks[0].filename, "myfile.txt");
}
