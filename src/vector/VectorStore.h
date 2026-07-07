#pragma once

#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

namespace fileagent
{

/**
 * @brief 内存向量存储 — 余弦相似度 + Top-K 检索
 *
 * 非持久化，服务重启后通过重新处理文件重建索引。
 * 线程安全（读写锁）。
 */
class VectorStore
{
public:
    /// 文档块记录
    struct Document
    {
        std::string chunk_id;       // 块唯一 ID（"fileId:chunkIndex"）
        std::int64_t file_id{0};    // 所属文件 ID
        std::string content;        // 块文本内容
        std::string filename;       // 原文件名
        std::vector<float> embedding; // 向量
        int chunk_index{0};         // 块序号
    };

    /// 检索结果
    struct SearchResult
    {
        std::int64_t file_id{0};
        std::string filename;
        std::string content;        // 匹配的文本片段
        float score{0.0f};          // 余弦相似度
        int chunk_index{0};
    };

    VectorStore() = default;
    ~VectorStore() = default;

    // 禁止拷贝
    VectorStore(const VectorStore &) = delete;
    VectorStore &operator=(const VectorStore &) = delete;

    /// 添加一个文档块到索引
    void addDocument(Document doc);

    /// 批量添加文档块
    void addDocuments(const std::vector<Document> &docs);

    /// 按 file_id 移除所有相关块
    void removeByFileId(std::int64_t file_id);

    /**
     * @brief 语义搜索
     * @param query_embedding 查询向量
     * @param top_k 返回 top K 结果
     * @return 按相似度降序排列的结果
     */
    std::vector<SearchResult> search(const std::vector<float> &query_embedding, int top_k = 5) const;

    /// 索引中的文档块总数
    size_t size() const;

    /// 清空所有索引
    void clear();

private:
    /// 计算两个向量之间的余弦相似度
    static float cosineSimilarity(const std::vector<float> &a, const std::vector<float> &b);

    std::vector<Document> index_;
    mutable std::shared_mutex rw_mutex_;
};

} // namespace fileagent
