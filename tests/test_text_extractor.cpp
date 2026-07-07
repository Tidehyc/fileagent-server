#include <gtest/gtest.h>

#include "common/TextExtractor.h"

using namespace fileagent;

TEST(TextExtractorTest, TextFile)
{
    auto result = TextExtractor::extract("hello world", "test.txt");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.text, "hello world");
}

TEST(TextExtractorTest, CodeFile)
{
    auto result = TextExtractor::extract("#include <iostream>\nint main() {}", "test.cpp");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.text, "#include <iostream>\nint main() {}");
}

TEST(TextExtractorTest, EmptyContent)
{
    auto result = TextExtractor::extract("", "test.txt");
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.text.empty());
}

TEST(TextExtractorTest, BinaryFile)
{
    auto result = TextExtractor::extract("fake binary content", "test.zip");
    EXPECT_FALSE(result.success);
}

TEST(TextExtractorTest, PdfFile)
{
    // 没有真实 PDF 时，pdftotext 会失败，返回 success=false
    auto result = TextExtractor::extract("%PDF-1.4 fake pdf content", "test.pdf");
    EXPECT_FALSE(result.success);
}

TEST(TextExtractorTest, IsTextFile)
{
    EXPECT_TRUE(TextExtractor::isTextFile("test.txt"));
    EXPECT_TRUE(TextExtractor::isTextFile("test.cpp"));
    EXPECT_TRUE(TextExtractor::isTextFile("test.h"));
    EXPECT_TRUE(TextExtractor::isTextFile("test.py"));
    EXPECT_TRUE(TextExtractor::isTextFile("test.md"));
    EXPECT_TRUE(TextExtractor::isTextFile("test.json"));
    EXPECT_FALSE(TextExtractor::isTextFile("test.zip"));
    EXPECT_FALSE(TextExtractor::isTextFile("test.pdf"));
    EXPECT_FALSE(TextExtractor::isTextFile("test"));
}

TEST(TextExtractorTest, IsPdfFile)
{
    EXPECT_TRUE(TextExtractor::isPdfFile("test.pdf"));
    EXPECT_FALSE(TextExtractor::isPdfFile("test.txt"));
}

TEST(TextExtractorTest, LargeFileTruncation)
{
    // 生成 200KB 文本
    std::string large(200 * 1024, 'A');
    auto result = TextExtractor::extract(large, "test.txt");
    EXPECT_TRUE(result.success);
    // 应被截断到 ~100KB
    EXPECT_LT(result.text.size(), 150 * 1024);
    EXPECT_NE(result.text.find("[内容已截断"), std::string::npos);
}

TEST(TextExtractorTest, NoExtension)
{
    auto result = TextExtractor::extract("some content", "README");
    EXPECT_FALSE(result.success);
}
