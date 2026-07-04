#pragma once

#include <cstddef>
#include <list>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <utility>

namespace fileagent
{

    /**
     * @brief 线程安全的 LRU (最近最少使用) 缓存
     *
     * @tparam K 键类型
     * @tparam V 值类型
     *
     * 使用 std::list 维护访问顺序（头=最近，尾=最旧）
     * + std::unordered_map 实现 O(1) 查找。
     * 使用 std::shared_mutex 实现读写分离：
     *   - get/exists 使用共享锁（读）
     *   - put/remove 使用独占锁（写）
     */
    template <typename K, typename V>
    class LruCache
    {
    public:
        using value_type = V;

        explicit LruCache(size_t capacity = 10000)
            : capacity_(capacity > 0 ? capacity : 1)
        {
        }

        /**
         * @brief 插入/更新缓存条目
         *
         * 若键已存在，更新值并移到最前。
         * 若缓存已满，淘汰最久未使用的条目。
         */
        void put(const K &key, V value)
        {
            std::unique_lock lock(mutex_);

            auto it = map_.find(key);
            if (it != map_.end())
            {
                // 键已存在：更新值，移到最前
                it->second->second = std::move(value);
                list_.splice(list_.begin(), list_, it->second);
                return;
            }

            // 淘汰最久未使用的条目
            if (list_.size() >= capacity_)
            {
                auto last = list_.end();
                --last;
                map_.erase(last->first);
                list_.pop_back();
            }

            // 插入新条目到最前
            list_.emplace_front(key, std::move(value));
            map_[key] = list_.begin();
        }

        /**
         * @brief 获取缓存条目
         *
         * 命中时将条目移到最前（标记为最近使用）。
         * @return std::optional<V> 未命中返回 std::nullopt
         */
        std::optional<V> get(const K &key)
        {
            std::unique_lock lock(mutex_);

            auto it = map_.find(key);
            if (it == map_.end())
            {
                return std::nullopt;
            }

            // 移到最前（标记为最近使用）
            list_.splice(list_.begin(), list_, it->second);
            return it->second->second;
        }

        /**
         * @brief 检查键是否存在（不移除访问顺序，比 get 开销小）
         */
        bool exists(const K &key) const
        {
            std::shared_lock lock(mutex_);
            return map_.find(key) != map_.end();
        }

        /**
         * @brief 删除指定键
         * @return true 键存在并被删除
         */
        bool remove(const K &key)
        {
            std::unique_lock lock(mutex_);

            auto it = map_.find(key);
            if (it == map_.end())
            {
                return false;
            }

            list_.erase(it->second);
            map_.erase(it);
            return true;
        }

        /**
         * @brief 清空所有缓存条目
         */
        void clear()
        {
            std::unique_lock lock(mutex_);
            list_.clear();
            map_.clear();
        }

        /**
         * @brief 当前缓存条目数
         */
        size_t size() const
        {
            std::shared_lock lock(mutex_);
            return list_.size();
        }

        /**
         * @brief 缓存容量上限
         */
        size_t capacity() const
        {
            return capacity_;
        }

    private:
        using ListNode = std::pair<K, V>;
        using ListIterator = typename std::list<ListNode>::iterator;

        std::list<ListNode> list_;                     // 从头(最近)到尾(最旧)
        std::unordered_map<K, ListIterator> map_;      // 快速索引
        mutable std::shared_mutex mutex_;               // 读写锁
        size_t capacity_;                               // 容量上限
    };

} // namespace fileagent
