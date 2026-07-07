#pragma once

#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

namespace fileagent
{

/**
 * @brief 简易滑动窗口限流器
 *
 * 统计每个 IP 在时间窗口内的请求次数，超过阈值则拒绝。
 * 线程安全（std::mutex）。
 */
class RateLimiter
{
public:
    /**
     * @param max_requests  时间窗口内允许的最大请求数
     * @param window_sec    时间窗口（秒）
     */
    explicit RateLimiter(int max_requests = 20, int window_sec = 60)
        : max_requests_(max_requests)
        , window_sec_(window_sec)
    {}

    /**
     * @brief 检查是否允许该 IP 继续请求
     * @param ip 客户端 IP
     * @return true=允许, false=被限流
     */
    bool allow(const std::string &ip)
    {
        auto now = Clock::now();

        std::lock_guard<std::mutex> lock(mutex_);

        auto &timestamps = records_[ip];

        // 移除超出窗口的时间戳
        auto cutoff = now - std::chrono::seconds(window_sec_);
        while (!timestamps.empty() && timestamps.front() < cutoff)
        {
            timestamps.pop_front();
        }

        // 检查是否超过限制
        if (static_cast<int>(timestamps.size()) >= max_requests_)
        {
            return false;
        }

        // 记录本次请求
        timestamps.push_back(now);
        return true;
    }

    /// 重置某个 IP 的计数（如登录成功后）
    void reset(const std::string &ip)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        records_.erase(ip);
    }

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    int max_requests_;
    int window_sec_;
    std::mutex mutex_;
    std::unordered_map<std::string, std::deque<TimePoint>> records_;
};

} // namespace fileagent
