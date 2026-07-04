#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include "common/Config.h"

namespace fileagent
{

    class MysqlConn;

    class ConnectionGuard
    {
    public:
        ConnectionGuard() = default;
        explicit ConnectionGuard(std::shared_ptr<MysqlConn> connection);

        MysqlConn *operator->() const noexcept;
        MysqlConn &operator*() const noexcept;
        MysqlConn *get() const noexcept;
        explicit operator bool() const noexcept;

    private:
        std::shared_ptr<MysqlConn> connection_;
    };

    class TransactionGuard
    {
    public:
        explicit TransactionGuard(ConnectionGuard connection);
        ~TransactionGuard();

        TransactionGuard(const TransactionGuard &) = delete;
        TransactionGuard &operator=(const TransactionGuard &) = delete;

        bool commit();
        MysqlConn *operator->() const noexcept;
        MysqlConn &operator*() const noexcept;
        explicit operator bool() const noexcept;

    private:
        ConnectionGuard connection_;
        bool committed_{false};
    };

    class ConnectionPool
    {
    public:
        static ConnectionPool &instance();

        bool init(const DatabaseConfig &config);
        ConnectionGuard getConnection();

        size_t available() const;
        size_t active() const;
        void shutdown();

        ConnectionPool(const ConnectionPool &) = delete;
        ConnectionPool &operator=(const ConnectionPool &) = delete;

    private:
        ConnectionPool() = default;
        ~ConnectionPool();

        bool addConnectionLocked();
        void producerLoop();
        void recyclerLoop();

        DatabaseConfig database_config_;
        std::queue<MysqlConn *> available_;
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        std::thread producer_thread_;
        std::thread recycler_thread_;
        std::atomic<size_t> active_count_{0};
        std::atomic<bool> running_{false};
        std::atomic<bool> initialized_{false};
    };

} // namespace fileagent