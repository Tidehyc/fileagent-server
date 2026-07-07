#include "VectorStore.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <numeric>
#include <shared_mutex>

namespace fileagent
{

void VectorStore::addDocument(Document doc)
{
    std::unique_lock lock(rw_mutex_);
    index_.push_back(std::move(doc));
}

void VectorStore::addDocuments(const std::vector<Document> &docs)
{
    std::unique_lock lock(rw_mutex_);
    index_.insert(index_.end(), docs.begin(), docs.end());
}

void VectorStore::removeByFileId(std::int64_t file_id)
{
    std::unique_lock lock(rw_mutex_);
    index_.erase(
        std::remove_if(index_.begin(), index_.end(),
                       [file_id](const Document &doc)
                       { return doc.file_id == file_id; }),
        index_.end());
}

std::vector<VectorStore::SearchResult> VectorStore::search(
    const std::vector<float> &query_embedding, int top_k) const
{
    std::shared_lock lock(rw_mutex_);

    if (index_.empty() || query_embedding.empty())
        return {};

    // 对所有文档块计算余弦相似度
    struct ScoredDoc
    {
        const Document *doc;
        float score;
    };

    std::vector<ScoredDoc> scored;
    scored.reserve(index_.size());

    for (const auto &doc : index_)
    {
        if (doc.embedding.empty())
            continue;
        float sim = cosineSimilarity(query_embedding, doc.embedding);
        scored.push_back({&doc, sim});
    }

    // 按分数降序排列
    std::sort(scored.begin(), scored.end(),
              [](const ScoredDoc &a, const ScoredDoc &b)
              { return a.score > b.score; });

    // 取 top_k
    std::vector<SearchResult> results;
    int count = std::min(top_k, static_cast<int>(scored.size()));
    for (int i = 0; i < count; ++i)
    {
        const auto &s = scored[i];
        SearchResult r;
        r.file_id = s.doc->file_id;
        r.filename = s.doc->filename;
        r.content = s.doc->content;
        r.score = s.score;
        r.chunk_index = s.doc->chunk_index;
        results.push_back(std::move(r));
    }

    return results;
}

size_t VectorStore::size() const
{
    std::shared_lock lock(rw_mutex_);
    return index_.size();
}

void VectorStore::clear()
{
    std::unique_lock lock(rw_mutex_);
    index_.clear();
}

float VectorStore::cosineSimilarity(const std::vector<float> &a, const std::vector<float> &b)
{
    if (a.size() != b.size() || a.empty())
        return 0.0f;

    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (size_t i = 0; i < a.size(); ++i)
    {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    float denom = std::sqrt(norm_a) * std::sqrt(norm_b);
    if (denom < 1e-10f)
        return 0.0f;

    return dot / denom;
}

} // namespace fileagent
