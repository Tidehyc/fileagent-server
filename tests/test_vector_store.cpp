#include <cmath>
#include <gtest/gtest.h>

#include "vector/VectorStore.h"

using namespace fileagent;

TEST(VectorStoreTest, EmptyStore)
{
    VectorStore store;
    EXPECT_EQ(store.size(), 0);

    auto results = store.search({1.0f, 0.0f}, 5);
    EXPECT_TRUE(results.empty());
}

TEST(VectorStoreTest, AddAndSearch)
{
    VectorStore store;

    VectorStore::Document doc1;
    doc1.chunk_id = "1:0";
    doc1.file_id = 1;
    doc1.content = "hello world";
    doc1.filename = "test.txt";
    doc1.embedding = {1.0f, 0.0f, 0.0f};
    doc1.chunk_index = 0;

    VectorStore::Document doc2;
    doc2.chunk_id = "2:0";
    doc2.file_id = 2;
    doc2.content = "goodbye world";
    doc2.filename = "other.txt";
    doc2.embedding = {0.0f, 1.0f, 0.0f};
    doc2.chunk_index = 0;

    store.addDocument(doc1);
    store.addDocument(doc2);

    EXPECT_EQ(store.size(), 2);

    auto results = store.search({0.9f, 0.1f, 0.0f}, 5);
    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].file_id, 1);
    EXPECT_GT(results[0].score, results[1].score);
}

TEST(VectorStoreTest, TopK)
{
    VectorStore store;

    // 创建指向不同方向的向量，余弦相似度递降
    for (int i = 0; i < 10; i++)
    {
        VectorStore::Document doc;
        doc.chunk_id = std::to_string(i) + ":0";
        doc.file_id = i;
        doc.content = "doc " + std::to_string(i);
        doc.filename = "doc.txt";
        float angle = static_cast<float>(i) * 0.174f; // i * 10°
        std::vector<float> emb(2);
        emb[0] = std::cos(angle);
        emb[1] = std::sin(angle);
        doc.embedding = emb;
        doc.chunk_index = 0;
        store.addDocument(std::move(doc));
    }

    // 搜索方向 {1,0}（0°）
    auto results = store.search({1.0f, 0.0f}, 3);
    EXPECT_EQ(results.size(), 3);
    // 最高分应该是 doc 0（0°，cos=1.0）
    EXPECT_EQ(results[0].file_id, 0);
    // 角度越大相似度越低
    EXPECT_GT(results[0].score, results[1].score);
    EXPECT_GT(results[1].score, results[2].score);
}

TEST(VectorStoreTest, RemoveByFileId)
{
    VectorStore store;

    VectorStore::Document doc;
    doc.chunk_id = "1:0";
    doc.file_id = 1;
    doc.content = "to be removed";
    doc.embedding = {1.0f, 0.0f};
    store.addDocument(std::move(doc));

    EXPECT_EQ(store.size(), 1);

    store.removeByFileId(1);
    EXPECT_EQ(store.size(), 0);
}

TEST(VectorStoreTest, AddMultipleDocuments)
{
    VectorStore store;

    std::vector<VectorStore::Document> docs;
    for (int i = 0; i < 5; i++)
    {
        VectorStore::Document doc;
        doc.chunk_id = std::to_string(i) + ":0";
        doc.file_id = i;
        doc.content = "doc " + std::to_string(i);
        doc.embedding = {1.0f};
        docs.push_back(std::move(doc));
    }

    store.addDocuments(docs);
    EXPECT_EQ(store.size(), 5);
}

TEST(VectorStoreTest, Clear)
{
    VectorStore store;
    VectorStore::Document doc;
    doc.chunk_id = "1:0";
    doc.file_id = 1;
    doc.embedding = {1.0f};
    store.addDocument(std::move(doc));

    EXPECT_EQ(store.size(), 1);
    store.clear();
    EXPECT_EQ(store.size(), 0);
}

TEST(VectorStoreTest, CosineSimilarity)
{
    VectorStore store;
    VectorStore::Document doc;
    doc.chunk_id = "1:0";
    doc.file_id = 1;
    doc.content = "test";
    doc.embedding = {1.0f, 0.0f};
    store.addDocument(std::move(doc));

    // 搜索匹配向量（cos=1.0）
    auto res = store.search({1.0f, 0.0f}, 5);
    EXPECT_EQ(res.size(), 1);
    EXPECT_FLOAT_EQ(res[0].score, 1.0f);

    // 搜索正交向量（cos=0.0）
    auto res2 = store.search({0.0f, 1.0f}, 5);
    EXPECT_EQ(res2.size(), 1);
    EXPECT_FLOAT_EQ(res2[0].score, 0.0f);
}
