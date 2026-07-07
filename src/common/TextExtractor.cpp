#include "TextExtractor.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include "Logger.h"

namespace fileagent
{

namespace
{

// 文本类扩展名列表
const std::array<const char *, 28> kTextExtensions = {
    "txt", "md", "markdown",
    "cpp", "cxx", "cc", "c", "h", "hpp", "hxx",
    "py", "js", "ts", "jsx", "tsx",
    "java", "rs", "go", "rb", "php",
    "html", "htm", "css", "scss", "less",
    "json", "yaml", "yml",
};

// 最大提取大小（100KB）
constexpr size_t kMaxExtractSize = 102400;

// 获取小写扩展名
std::string lowercaseExt(const std::string &filename)
{
    auto pos = filename.find_last_of('.');
    if (pos == std::string::npos || pos == filename.size() - 1)
        return "";

    std::string ext = filename.substr(pos + 1);
    for (auto &c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

bool writeTempFile(const std::string &content, std::string &out_path)
{
    char temp_path[] = "/tmp/fileagent_XXXXXX";
    int fd = mkstemp(temp_path);
    if (fd == -1)
        return false;

    FILE *f = fdopen(fd, "wb");
    if (!f)
    {
        close(fd);
        unlink(temp_path);
        return false;
    }

    size_t written = fwrite(content.data(), 1, content.size(), f);
    int fclose_ret = fclose(f);

    if (written != content.size() || fclose_ret != 0)
    {
        unlink(temp_path);
        return false;
    }

    out_path = temp_path;
    return true;
}

} // anonymous namespace

bool TextExtractor::isTextFile(const std::string &filename)
{
    std::string ext = lowercaseExt(filename);
    for (const auto &text_ext : kTextExtensions)
    {
        if (ext == text_ext)
            return true;
    }
    return false;
}

bool TextExtractor::isPdfFile(const std::string &filename)
{
    return lowercaseExt(filename) == "pdf";
}

TextExtractor::ExtractResult TextExtractor::extract(const std::string &content, const std::string &filename)
{
    if (content.empty())
    {
        return ExtractResult{"", "application/octet-stream", false};
    }

    if (isPdfFile(filename))
    {
        return extractPdf(content, filename);
    }

    if (isTextFile(filename))
    {
        return extractText(content, filename);
    }

    // 不支持的文件类型
    return ExtractResult{"", "application/octet-stream", false};
}

TextExtractor::ExtractResult TextExtractor::extractPdf(const std::string &content, const std::string &filename)
{
    (void)filename;

    // 将 PDF 内容写入临时文件，调用 pdftotext 提取
    std::string temp_path;
    if (!writeTempFile(content, temp_path))
    {
        logWarn("TextExtractor: failed to write temp file for PDF");
        return ExtractResult{"", "application/pdf", false};
    }

    // 构造 pdftotext 命令
    std::string cmd = "pdftotext -nopgbrk \"" + temp_path + "\" - 2>/dev/null";

    FILE *fp = popen(cmd.c_str(), "r");
    if (!fp)
    {
        unlink(temp_path.c_str());
        return ExtractResult{"", "application/pdf", false};
    }

    std::string result;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp) != nullptr)
    {
        result += buf;
        if (result.size() > kMaxExtractSize)
        {
            result += "\n\n[内容已截断，仅提取前 100KB]";
            break;
        }
    }

    int status = pclose(fp);
    unlink(temp_path.c_str());

    if (status != 0 || result.empty())
    {
        logWarn("TextExtractor: pdftotext returned empty or failed");
        return ExtractResult{"", "application/pdf", false};
    }

    return ExtractResult{result, "application/pdf", true};
}

TextExtractor::ExtractResult TextExtractor::extractText(const std::string &content, const std::string &filename)
{
    std::string text = content;

    // 截断
    if (text.size() > kMaxExtractSize)
    {
        text.resize(kMaxExtractSize);
        text += "\n\n[内容已截断，仅保留前 100KB]";
    }

    std::string mime = "text/plain";
    std::string ext = lowercaseExt(filename);

    // 根据扩展名设更精确的 MIME
    if (ext == "json") mime = "application/json";
    else if (ext == "html" || ext == "htm") mime = "text/html";
    else if (ext == "md" || ext == "markdown") mime = "text/markdown";
    else if (ext == "yaml" || ext == "yml") mime = "text/yaml";
    else if (ext == "cpp" || ext == "cxx" || ext == "cc" || ext == "c" || ext == "h" || ext == "hpp") mime = "text/x-c++";
    else if (ext == "py") mime = "text/x-python";
    else if (ext == "js" || ext == "ts") mime = "text/javascript";
    else if (ext == "java") mime = "text/x-java";
    else if (ext == "rs") mime = "text/x-rust";
    else if (ext == "go") mime = "text/x-go";
    else if (ext == "rb") mime = "text/x-ruby";
    else if (ext == "css") mime = "text/css";

    return ExtractResult{text, mime, true};
}

} // namespace fileagent
