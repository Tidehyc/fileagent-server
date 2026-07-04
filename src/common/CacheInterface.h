#pragma once

#include <cstddef>
#include <optional>

namespace fileagent
{

    /**
     * @brief 缓存通用抽象接口
     *
     * 模板类 + 虚方法：C++ 允许每个模板实例化拥有自己的虚表，
     * 通过这个接口，CacheManager 可以运行时切换 LRU / Redis 实现。
     */
    template <typename K, typename V>
    class CacheInterface
    {
    public:
        virtual ~CacheInterface() = default;

        /**
         * @brief 插入/更新缓存条目
         */
        virtual void put(const K &key, V value) = 0;

        /**
         * @brief 获取缓存条目
         * @return 未命中返回 std::nullopt
         */
        virtual std::optional<V> get(const K &key) = 0;

        /**
         * @brief 删除指定键
         * @return true 键存在并被删除
         */
        virtual bool remove(const K &key) = 0;

        /**
         * @brief 清空所有缓存条目
         */
        virtual void clear() = 0;

        /**
         * @brief 当前缓存条目数
         */
        virtual size_t size() const = 0;
    };

} // namespace fileagent
