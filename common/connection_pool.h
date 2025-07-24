#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H

#include "network.h"
#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <atomic>
#include <string>
#include <iomanip>
#include <netinet/tcp.h>

class ConnectionPool {
public:
    // 配置结构体 - 移到类的最前面
    struct Config {
        size_t max_connections_per_host = 10;        // 每个主机的最大连接数
        size_t min_connections_per_host = 2;         // 每个主机的最小连接数
        std::chrono::seconds connection_timeout{10}; // 连接建立超时时间
        std::chrono::seconds idle_timeout{300};      // 空闲连接超时时间（5分钟）
        std::chrono::seconds acquire_timeout{5};     // 获取连接的超时时间
    };
    
    // 构造和析构 - 提供两个版本避免默认参数问题
    ConnectionPool();
    explicit ConnectionPool(const Config& config);
    ~ConnectionPool();
    
    // 禁用拷贝和赋值
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&) = delete;
    ConnectionPool& operator=(ConnectionPool&&) = delete;
    
    // 主要接口
    std::shared_ptr<TcpConnection> acquire(const std::string& host, int port);
    void release(const std::string& host, int port, std::shared_ptr<TcpConnection> conn);
    
    // 预热连接池
    void warmup(const std::string& host, int port, size_t count = 0);
    
    // 获取统计信息
    struct Stats {
        size_t total_connections = 0;    // 总连接数
        size_t active_connections = 0;   // 活跃连接数
        size_t idle_connections = 0;     // 空闲连接数
        size_t hosts_count = 0;          // 主机数量
    };
    Stats getStats() const;
    
    // 打印统计信息
    void printStats() const;
    
    // 手动清理过期连接
    void cleanup();
    
    // 停止连接池
    void stop();

private:
    // 池化连接结构
    struct PooledConnection {
        std::shared_ptr<TcpConnection> connection;
        std::chrono::steady_clock::time_point last_used;
        bool in_use = false;
        
        explicit PooledConnection(std::shared_ptr<TcpConnection> conn)
            : connection(std::move(conn)), last_used(std::chrono::steady_clock::now()) {}
    };
    
    // 主机连接池结构
    struct HostPool {
        // 空闲连接队列
        std::queue<std::shared_ptr<PooledConnection>> idle_connections;
        
        // 所有连接（包括活跃和空闲）
        std::vector<std::shared_ptr<PooledConnection>> all_connections;
        
        // 同步原语
        mutable std::mutex mutex;
        std::condition_variable condition;
        
        // 活跃连接计数
        size_t active_count = 0;
        
        // 统计方法
        size_t getTotalConnections() const { return all_connections.size(); }
        size_t getIdleConnections() const { return idle_connections.size(); }
        size_t getActiveConnections() const { return active_count; }
    };
    
    // 成员变量
    Config config_;                                              // 配置
    std::unordered_map<std::string, std::unique_ptr<HostPool>> pools_;  // 主机池映射
    mutable std::shared_mutex pools_mutex_;                     // 池映射保护锁
    
    std::thread cleanup_thread_;                                // 清理线程
    std::atomic<bool> running_;                                 // 运行状态
    
    // 私有方法 - 工具函数
    std::string makeKey(const std::string& host, int port) const;
    HostPool* getOrCreateHostPool(const std::string& key);
    
    // 私有方法 - 连接管理
    std::shared_ptr<TcpConnection> createConnection(const std::string& host, int port);
    bool isConnectionValid(const std::shared_ptr<TcpConnection>& conn);
    void removeConnection(HostPool* pool, std::shared_ptr<PooledConnection> pooled_conn);
    
    // 私有方法 - 清理
    void cleanupExpiredConnections();
    void runCleanupLoop();
};

#endif // CONNECTION_POOL_H