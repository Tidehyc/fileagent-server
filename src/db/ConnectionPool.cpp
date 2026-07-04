#include "ConnectionPool.h"

#include <chrono>
#include <memory>
#include <utility>

#include "MysqlConn.h"
#include "common/Logger.h"

namespace fileagent
{

    ConnectionGuard::ConnectionGuard(std::shared_ptr<MysqlConn> connection)
        : connection_(std::move(connection))
    {
    }

    MysqlConn *ConnectionGuard::operator->() const noexcept
    {
        return connection_.get();
    }

    MysqlConn &ConnectionGuard::operator*() const noexcept
    {
        return *connection_;
    }

    MysqlConn *ConnectionGuard::get() const noexcept
    {
        return connection_.get();
    }

    ConnectionGuard::operator bool() const noexcept
    {
        return static_cast<bool>(connection_);
    }

    TransactionGuard::TransactionGuard(ConnectionGuard connection)
        : connection_(std::move(connection))
    {
        if (connection_)
        {
            connection_->transaction();
        }
    }

    TransactionGuard::~TransactionGuard()
    {
        if (connection_ && !committed_)
        {
            connection_->rollback();
        }
    }

    bool TransactionGuard::commit()
    {
        if (!connection_)
        {
            return false;
        }

        committed_ = connection_->commit();
        return committed_;
    }

    MysqlConn *TransactionGuard::operator->() const noexcept
    {
        return connection_.get();
    }

    MysqlConn &TransactionGuard::operator*() const noexcept
    {
        return *connection_.get();
    }

    TransactionGuard::operator bool() const noexcept
    {
        return static_cast<bool>(connection_);
    }

    ConnectionPool &ConnectionPool::instance()
    {
        static ConnectionPool pool;
        return pool;
    }

    ConnectionPool::~ConnectionPool()
    {
        shutdown();
    }

    bool ConnectionPool::init(const DatabaseConfig &config)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_)
        {
            return true;
        }

        database_config_ = config;
        running_ = true;

        for (size_t index = 0; index < database_config_.pool.min_size; ++index)
        {
            if (!addConnectionLocked())
            {
                running_ = false;
                while (!available_.empty())
                {
                    delete available_.front();
                    available_.pop();
                }
                return false;
            }
        }

        producer_thread_ = std::thread(&ConnectionPool::producerLoop, this);
        recycler_thread_ = std::thread(&ConnectionPool::recyclerLoop, this);
        initialized_ = true;
        return true;
    }

    ConnectionGuard ConnectionPool::getConnection()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock,
                     database_config_.pool.timeout,
                     [this]
                     {
                         return !running_ || !available_.empty();
                     });

        if (!running_ || available_.empty())
        {
            return ConnectionGuard{};
        }

        MysqlConn *connection = available_.front();
        available_.pop();
        active_count_++;

        auto deleter = [this](MysqlConn *conn)
        {
            if (conn == nullptr)
            {
                return;
            }

            std::unique_lock<std::mutex> lock(mutex_);
            conn->refreshAliveTime();
            if (running_ && available_.size() < database_config_.pool.max_size)
            {
                available_.push(conn);
            }
            else
            {
                delete conn;
            }

            if (active_count_ > 0)
            {
                active_count_--;
            }
            cv_.notify_one();
        };

        return ConnectionGuard(std::shared_ptr<MysqlConn>(connection, deleter));
    }

    size_t ConnectionPool::available() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return available_.size();
    }

    size_t ConnectionPool::active() const
    {
        return active_count_.load();
    }

    void ConnectionPool::shutdown()
    {
        running_ = false;
        cv_.notify_all();

        if (producer_thread_.joinable())
        {
            producer_thread_.join();
        }
        if (recycler_thread_.joinable())
        {
            recycler_thread_.join();
        }

        std::lock_guard<std::mutex> lock(mutex_);
        while (!available_.empty())
        {
            delete available_.front();
            available_.pop();
        }
        initialized_ = false;
    }

    bool ConnectionPool::addConnectionLocked()
    {
        auto connection = std::make_unique<MysqlConn>();
        if (!connection->connect(database_config_.user,
                                 database_config_.password,
                                 database_config_.database,
                                 database_config_.host,
                                 database_config_.port))
        {
            logError("Failed to create MySQL connection for pool");
            return false;
        }

        connection->refreshAliveTime();
        available_.push(connection.release());
        cv_.notify_one();
        return true;
    }

    void ConnectionPool::producerLoop()
    {
        while (running_)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock,
                         std::chrono::seconds(1),
                         [this]
                         {
                             return !running_ || available_.size() < database_config_.pool.min_size;
                         });

            if (!running_)
            {
                break;
            }

            while (running_ && available_.size() < database_config_.pool.min_size)
            {
                if (!addConnectionLocked())
                {
                    break;
                }
            }
        }
    }

    void ConnectionPool::recyclerLoop()
    {
        while (running_)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            std::lock_guard<std::mutex> lock(mutex_);
            while (available_.size() > database_config_.pool.min_size)
            {
                MysqlConn *connection = available_.front();
                if (connection->getAliveTime() >= static_cast<long long>(database_config_.pool.idle_timeout.count() * 1000))
                {
                    available_.pop();
                    delete connection;
                }
                else
                {
                    break;
                }
            }
        }
    }

} // namespace fileagent